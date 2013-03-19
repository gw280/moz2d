/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestDrawTargetBase.h"
#include <sstream>

using namespace mozilla;
using namespace mozilla::gfx;
using namespace std;

TestDrawTargetBase::TestDrawTargetBase()
  : mFlush(nullptr)
{
  REGISTER_TEST(TestDrawTargetBase, FillRect50x50x500);
  REGISTER_TEST(TestDrawTargetBase, FillRect200x200x500);
  REGISTER_TEST(TestDrawTargetBase, FillRect50x50x2000);
  REGISTER_TEST(TestDrawTargetBase, FillRect200x200x2000);
  REGISTER_TEST(TestDrawTargetBase, FillRect50x50x500Add);
  REGISTER_TEST(TestDrawTargetBase, FillRect200x200x500Add);
  REGISTER_TEST(TestDrawTargetBase, FillRect50x50x2000Add);
  REGISTER_TEST(TestDrawTargetBase, FillRect200x200x2000Add);
}

void
TestDrawTargetBase::FillSquare(int aSize, int aRepeat, CompositionOp aOp)
{
  for (int i = 0; i < aRepeat; i++) {
    mDT->FillRect(Rect(i / 6, i / 4, aSize, aSize), ColorPattern(Color(1.0f, 0, 0, 1.0f)), DrawOptions(0.5f, aOp));
    if (mFlush) {
      mFlush(this);
    }
  }
}

void
TestDrawTargetBase::FillRect50x50x500()
{
  FillSquare(50, 500);
}

void
TestDrawTargetBase::FillRect200x200x500()
{
  FillSquare(200, 500);
}

void
TestDrawTargetBase::FillRect50x50x2000()
{
  FillSquare(50, 2000);
}

void
TestDrawTargetBase::FillRect200x200x2000()
{
  FillSquare(200, 2000);
}

void
TestDrawTargetBase::FillRect50x50x500Add()
{
  FillSquare(50, 500, OP_ADD);
}

void
TestDrawTargetBase::FillRect200x200x500Add()
{
  FillSquare(200, 500, OP_ADD);
}

void
TestDrawTargetBase::FillRect50x50x2000Add()
{
  FillSquare(50, 2000, OP_ADD);
}

void
TestDrawTargetBase::FillRect200x200x2000Add()
{
  FillSquare(200, 2000, OP_ADD);
}
