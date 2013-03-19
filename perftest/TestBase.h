/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include <string>
#include <vector>

#ifdef _MSC_VER
// On MSVC otherwise our generic member pointer trick doesn't work.
#pragma pointers_to_members(full_generality, single_inheritance)
#include <Windows.h>
#else
#include <pthread.h>
#endif

inline void SleepMS(int aMilliseconds)
{
#ifdef _MSC_VER
  ::Sleep(aMilliseconds);
#else
  sleep(aMilliseconds);
#endif
}

class HighPrecisionMeasurement
{
public:
  void Start() {
#ifdef WIN32
    ::QueryPerformanceCounter(&mStart);
#endif
  }

  double Measure() {
#ifdef WIN32
    LARGE_INTEGER end, freq;
    ::QueryPerformanceCounter(&end);
    ::QueryPerformanceFrequency(&freq);
    return (double(end.QuadPart) - double(mStart.QuadPart)) / double(freq.QuadPart) * 1000.00;
#endif
    return 0;
  }
private:
#ifdef WIN32
  LARGE_INTEGER mStart;
#endif
};

#define REGISTER_TEST(className, testName) \
  mTests.push_back(Test(static_cast<TestCall>(&className::testName), #testName, this))

class TestBase
{
public:
  TestBase() {}

  typedef void (TestBase::*TestCall)();

  int RunTests();

protected:
  static void LogMessage(std::string aMessage);

  struct Test {
    Test(TestCall aCall, std::string aName, void *aImplPointer)
      : funcCall(aCall)
      , name(aName)
      , implPointer(aImplPointer)
    {
    }
    TestCall funcCall;
    std::string name;
    void *implPointer;
  };
  std::vector<Test> mTests;

private:
  // This doesn't really work with our generic member pointer trick.
  TestBase(const TestBase &aOther);
};
