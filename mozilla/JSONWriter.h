/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A JSON pretty-printer class. */

// A typical JSON-writing library requires you to first build up a data
// structure that represents a JSON object and then serialize it (to file, or
// somewhere else). This approach makes for a clean API, but building the data
// structure takes up memory. Sometimes that isn't desirable, such as when the
// JSON data is produced for memory reporting.
//
// The JSONWriter class instead allows JSON data to be written out
// incrementally without building up large data structures.
//
// The API is slightly uglier than you would see in a typical JSON-writing
// library, but still fairly easy to use. It's possible to generate invalid
// JSON with JSONWriter, but typically the most basic testing will identify any
// such problems.
//
// Similarly, there are no RAII facilities for automatically closing objects
// and arrays. These would be nice if you are generating all your code within
// nested functions, but in other cases you'd have to maintain an explicit
// stack of RAII objects and manually unwind it, which is no better than just
// calling "end" functions. Furthermore, the consequences of forgetting to
// close an object or array are obvious and, again, will be identified via
// basic testing, unlike other cases where RAII is typically used (e.g. smart
// pointers) and the consequences of defects are more subtle.
//
// Importantly, the class does solve the two hard problems of JSON
// pretty-printing, which are (a) correctly escaping strings, and (b) adding
// appropriate indentation and commas between items.
//
// Strings used (for property names and string property values) are |const
// char*| throughout, and can be ASCII or UTF-8.
//
// EXAMPLE
// -------
// Assume that |MyWriteFunc| is a class that implements |JSONWriteFunc|. The
// following code:
//
//   JSONWriter w(MakeUnique<MyWriteFunc>());
//   w.Start();
//   {
//     w.NullProperty("null");
//     w.BoolProperty("bool", true);
//     w.IntProperty("int", 1);
//     w.StringProperty("string", "hello");
//     w.StartArrayProperty("array");
//     {
//       w.DoubleElement(3.4);
//       w.StartObjectElement();
//       {
//         w.PointerProperty("ptr", (void*)0x12345678);
//       }
//       w.EndObjectElement();
//     }
//     w.EndArrayProperty();
//   }
//   w.End();
//
// will produce pretty-printed output for the following JSON object:
//
//  {
//   "null": null,
//   "bool": true,
//   "int": 1,
//   "string": "hello",
//   "array": [
//    3.4,
//    {
//     "ptr": "0x12345678"
//    }
//   ]
//  }
//
// The nesting in the example code is obviously optional, but can aid
// readability.

#ifndef mozilla_JSONWriter_h
#define mozilla_JSONWriter_h

#include "mozilla/double-conversion.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/PodOperations.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"

#include <stdio.h>

namespace mozilla {

// A quasi-functor for JSONWriter. We don't use a true functor because that
// requires templatizing JSONWriter, and the templatization seeps to lots of
// places we don't want it to.
class JSONWriteFunc
{
public:
  virtual void Write(const char* aStr) = 0;
  virtual ~JSONWriteFunc() {}
};

// Ideally this would be within |EscapedString| but when compiling with GCC
// on Linux that caused link errors, whereas this formulation didn't.
namespace detail {
extern MFBT_DATA const char gTwoCharEscapes[256];
}

class JSONWriter
{
  // From http://www.ietf.org/rfc/rfc4627.txt:
  //
  //   "All Unicode characters may be placed within the quotation marks except
  //   for the characters that must be escaped: quotation mark, reverse
  //   solidus, and the control characters (U+0000 through U+001F)."
  //
  // This implementation uses two-char escape sequences where possible, namely:
  //
  //   \", \\, \b, \f, \n, \r, \t
  //
  // All control characters not in the above list are represented with a
  // six-char escape sequence, e.g. '\u000b' (a.k.a. '\v').
  //
  class EscapedString
  {
    // Only one of |mUnownedStr| and |mOwnedStr| are ever non-null. |mIsOwned|
    // indicates which one is in use. They're not within a union because that
    // wouldn't work with UniquePtr.
    bool mIsOwned;
    const char* mUnownedStr;
    UniquePtr<char[]> mOwnedStr;

    void SanityCheck() const
    {
      MOZ_ASSERT_IF( mIsOwned,  mOwnedStr.get() && !mUnownedStr);
      MOZ_ASSERT_IF(!mIsOwned, !mOwnedStr.get() &&  mUnownedStr);
    }

    static char hexDigitToAsciiChar(uint8_t u)
    {
      u = u & 0xf;
      return u < 10 ? '0' + u : 'a' + (u - 10);
    }

  public:
    EscapedString(const char* aStr)
      : mUnownedStr(nullptr)
      , mOwnedStr(nullptr)
    {
      const char* p;

      // First, see if we need to modify the string.
      size_t nExtra = 0;
      p = aStr;
      while (true) {
        uint8_t u = *p;   // ensure it can't be interpreted as negative
        if (u == 0) {
          break;
        }
        if (detail::gTwoCharEscapes[u]) {
          nExtra += 1;
        } else if (u <= 0x1f) {
          nExtra += 5;
        }
        p++;
      }

      if (nExtra == 0) {
        // No escapes needed. Easy.
        mIsOwned = false;
        mUnownedStr = aStr;
        return;
      }

      // Escapes are needed. We'll create a new string.
      mIsOwned = true;
      size_t len = (p - aStr) + nExtra;
      mOwnedStr = MakeUnique<char[]>(len + 1);

      p = aStr;
      size_t i = 0;

      while (true) {
        uint8_t u = *p;   // ensure it can't be interpreted as negative
        if (u == 0) {
          mOwnedStr[i] = 0;
          break;
        }
        if (detail::gTwoCharEscapes[u]) {
          mOwnedStr[i++] = '\\';
          mOwnedStr[i++] = detail::gTwoCharEscapes[u];
        } else if (u <= 0x1f) {
          mOwnedStr[i++] = '\\';
          mOwnedStr[i++] = 'u';
          mOwnedStr[i++] = '0';
          mOwnedStr[i++] = '0';
          mOwnedStr[i++] = hexDigitToAsciiChar((u & 0x00f0) >> 4);
          mOwnedStr[i++] = hexDigitToAsciiChar(u & 0x000f);
        } else {
          mOwnedStr[i++] = u;
        }
        p++;
      }
    }

    ~EscapedString()
    {
      SanityCheck();
    }

    const char* get() const
    {
      SanityCheck();
      return mIsOwned ? mOwnedStr.get() : mUnownedStr;
    }
  };

  const UniquePtr<JSONWriteFunc> mWriter;
  Vector<bool, 8> mNeedComma;     // do we need a comma at depth N?
  size_t mDepth;                  // the current nesting depth

  void Indent()
  {
    for (size_t i = 0; i < mDepth; i++) {
      mWriter->Write(" ");
    }
  }

  // Adds whatever is necessary (maybe a comma, and then a newline and
  // whitespace) to separate an item (property or element) from what's come
  // before.
  void Separator()
  {
    if (mNeedComma[mDepth]) {
      mWriter->Write(",");
    }
    if (mDepth > 0) {
      mWriter->Write("\n");
    }
    Indent();
  }

  void PropertyNameAndColon(const char* aName)
  {
    EscapedString escapedName(aName);
    mWriter->Write("\"");
    mWriter->Write(escapedName.get());
    mWriter->Write("\": ");
  }

  void Scalar(const char* aMaybePropertyName, const char* aStringValue)
  {
    Separator();
    if (aMaybePropertyName) {
      PropertyNameAndColon(aMaybePropertyName);
    }
    mWriter->Write(aStringValue);
    mNeedComma[mDepth] = true;
  }

  void QuotedScalar(const char* aMaybePropertyName, const char* aStringValue)
  {
    Separator();
    if (aMaybePropertyName) {
      PropertyNameAndColon(aMaybePropertyName);
    }
    mWriter->Write("\"");
    mWriter->Write(aStringValue);
    mWriter->Write("\"");
    mNeedComma[mDepth] = true;
  }

  void NewCommaEntry()
  {
    // If this tiny allocation OOMs we might as well just crash because we must
    // be in serious memory trouble.
    MOZ_RELEASE_ASSERT(mNeedComma.growByUninitialized(1));
    mNeedComma[mDepth] = false;
  }

  void StartCollection(const char* aMaybePropertyName, const char* aStartChar)
  {
    Separator();
    if (aMaybePropertyName) {
      mWriter->Write("\"");
      mWriter->Write(aMaybePropertyName);
      mWriter->Write("\": ");
    }
    mWriter->Write(aStartChar);
    mNeedComma[mDepth] = true;
    mDepth++;
    NewCommaEntry();
  }

  // Adds the whitespace and closing char necessary to end a collection.
  void EndCollection(const char* aEndChar)
  {
    mDepth--;
    mWriter->Write("\n");
    Indent();
    mWriter->Write(aEndChar);
  }

public:
  explicit JSONWriter(UniquePtr<JSONWriteFunc> aWriter)
    : mWriter(Move(aWriter))
    , mNeedComma()
    , mDepth(0)
  {
    NewCommaEntry();
  }

  // Returns the JSONWriteFunc passed in at creation, for temporary use. The
  // JSONWriter object still owns the JSONWriteFunc.
  JSONWriteFunc* WriteFunc() const { return mWriter.get(); }

  // For all the following functions, the "Prints:" comment indicates what the
  // basic output looks like. However, it doesn't indicate the indentation and
  // trailing commas, which are automatically added as required.
  //
  // All property names and string properties are escaped as necessary.

  // Prints: {
  void Start() { StartCollection(nullptr, "{"); }

  // Prints: }\n
  void End() { EndCollection("}\n"); }

  // Prints: "<aName>": null
  void NullProperty(const char* aName)
  {
    Scalar(aName, "null");
  }

  // Prints: null
  void NullElement() { NullProperty(nullptr); }

  // Prints: "<aName>": <aBool>
  void BoolProperty(const char* aName, bool aBool)
  {
    Scalar(aName, aBool ? "true" : "false");
  }

  // Prints: <aBool>
  void BoolElement(bool aBool) { BoolProperty(nullptr, aBool); }

  // Prints: "<aName>": <aInt>
  void IntProperty(const char* aName, int64_t aInt)
  {
    char buf[64];
    sprintf(buf, "%" PRId64, aInt);
    Scalar(aName, buf);
  }

  // Prints: <aInt>
  void IntElement(int64_t aInt) { IntProperty(nullptr, aInt); }

  // Prints: "<aName>": <aDouble>
  void DoubleProperty(const char* aName, double aDouble)
  {
    static const size_t buflen = 64;
    char buf[buflen];
    const double_conversion::DoubleToStringConverter &converter =
      double_conversion::DoubleToStringConverter::EcmaScriptConverter();
    double_conversion::StringBuilder builder(buf, buflen);
    converter.ToShortest(aDouble, &builder);
    Scalar(aName, builder.Finalize());
  }

  // Prints: <aDouble>
  void DoubleElement(double aDouble) { DoubleProperty(nullptr, aDouble); }

  // Prints: "<aName>": "<aStr>"
  void StringProperty(const char* aName, const char* aStr)
  {
    EscapedString escapedStr(aStr);
    QuotedScalar(aName, escapedStr.get());
  }

  // Prints: "<aStr>"
  void StringElement(const char* aStr) { StringProperty(nullptr, aStr); }

  // Prints: "<aName>": "<aPtr>"
  // The pointer is printed as a hexadecimal integer with a leading '0x'.
  void PointerProperty(const char* aName, const void* aPtr)
  {
    char buf[32];
    sprintf(buf, "0x%" PRIxPTR, uintptr_t(aPtr));
    QuotedScalar(aName, buf);
  }

  // Prints: "<aPtr>"
  // The pointer is printed as a hexadecimal integer with a leading '0x'.
  void PointerElement(const void* aPtr) { PointerProperty(nullptr, aPtr); }

  // Prints: "<aName>": [
  void StartArrayProperty(const char* aName) { StartCollection(aName, "["); }

  // Prints: [
  void StartArrayElement() { StartArrayProperty(nullptr); }

  // Prints: ]
  void EndArray() { EndCollection("]"); }

  // Prints: "<aName>": {
  void StartObjectProperty(const char* aName) { StartCollection(aName, "{"); }

  // Prints: {
  void StartObjectElement() { StartObjectProperty(nullptr); }

  // Prints: }
  void EndObject() { EndCollection("}"); }
};

} // namespace mozilla

#endif /* mozilla_JSONWriter_h */

