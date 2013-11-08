/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SanityChecks.h"
#include "TestPoint.h"
#include "TestRect.h"
#include "TestScaling.h"
#ifdef WIN32
#include <d3d10_1.h>
#endif
#include "TestDrawTarget.h"

#include <string>
#include <sstream>

struct TestObject {
  TestBase *test;
  std::string name;
};


using namespace std;
using namespace mozilla;
using namespace mozilla::gfx;

int
main()
{
  RefPtr<ID3D10Device1> mDevice;
#ifdef WIN32
  ::D3D10CreateDevice1(nullptr,
                       D3D10_DRIVER_TYPE_HARDWARE,
                       nullptr,
                       D3D10_CREATE_DEVICE_BGRA_SUPPORT |
                       D3D10_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS,
                       D3D10_FEATURE_LEVEL_10_0,
                       D3D10_1_SDK_VERSION,
                       byRef(mDevice));

  Factory::SetDirect3D10Device(mDevice);
#endif

  TestObject tests[] = 
  {
    { new SanityChecks(), "Sanity Checks" },
  #ifdef WIN32
    { new TestDrawTargetD2D(), "DrawTarget (D2D)" },
  #endif
  #ifdef USE_CAIRO
    { new TestDrawTargetCairoImage(), "DrawTarget (Cairo Image)" },
  #endif
  #ifdef USE_SKIA
    { new TestDrawTargetSkiaSoftware(), "DrawTarget (Skia Software)" },
  #endif
    { new TestPoint(), "Point Tests" },
    { new TestRect(), "Rect Tests" },
    { new TestScaling(), "Scaling Tests" }
  };

  int totalFailures = 0;
  int totalTests = 0;
  stringstream message;
  printf("------ STARTING RUNNING TESTS ------\n");
  for (int i = 0; i < sizeof(tests) / sizeof(TestObject); i++) {
    message << "--- RUNNING TESTS: " << tests[i].name << " ---\n";
    printf(message.str().c_str());
    message.str("");
    int failures = 0;
    totalTests += tests[i].test->RunTests(&failures);
    totalFailures += failures;
    // Done with this test!
    delete tests[i].test;
  }
  message << "------ FINISHED RUNNING TESTS ------\nTests run: " << totalTests << " - Passes: " << totalTests - totalFailures << " - Failures: " << totalFailures << "\n";
  printf(message.str().c_str());
  return totalFailures;
}
