// recordbench.cpp : Defines the entry point for the console application.
//

#include <fstream>
#ifdef WIN32
#include <d3d10_1.h>
#endif
#include "2D.h"
#include "RecordedEvent.h"
#include "RawTranslator.h"
#include "perftest/TestBase.h"

#include <string>

using namespace mozilla;
using namespace mozilla::gfx;
using namespace std;

BackendType sTestedBackends[] =
{
  BACKEND_DIRECT2D
#ifdef USE_SKIA
  , BACKEND_SKIA
#endif
};

string
GetBackendName(BackendType aType)
{
  switch(aType) {
  case BACKEND_DIRECT2D:
    return "Direct2D";
  case BACKEND_SKIA:
    return "Skia";
  case BACKEND_CAIRO:
    return "Cairo";
  default:
    return "Unknown";
  }
}

const int sN = 10;

int
main(int argc, char *argv[], char *envp[])
{
  if (argc < 2) {
    printf("No recording specified.");
    return 1;
  }

  struct EventWithID {
    RecordedEvent* recordedEvent;
    uint32_t eventID;
  };

  vector<EventWithID > drawingEvents;
  vector<EventWithID > fontCreations;

  ifstream inputFile;

  inputFile.open(argv[1], istream::in | istream::binary);

  inputFile.seekg(0, ios::end);
  int length = inputFile.tellg();
  inputFile.seekg(0, ios::beg);

  uint32_t magicInt;
  ReadElement(inputFile, magicInt);
  if (magicInt != 0xc001feed) {
    printf("File is not a valid recording");
    return 1;
  }

  uint16_t majorRevision;
  uint16_t minorRevision;
  ReadElement(inputFile, majorRevision);
  ReadElement(inputFile, minorRevision);

  if (majorRevision != kMajorRevision) {
    printf("Recording was made with a different major revision");
    return 1;
  }

  if (minorRevision > kMinorRevision) {
    printf("Recording was made with a later minor revision");
    return 1;
  }

  uint32_t eventIndex = 0;
  while (inputFile.tellg() < length) {
    int32_t type;
    ReadElement(inputFile, type);

    EventWithID newEvent;
    newEvent.recordedEvent = RecordedEvent::LoadEventFromStream(inputFile, (RecordedEvent::EventType)type);
    newEvent.eventID = eventIndex++;
    if (newEvent.recordedEvent->GetType() == RecordedEvent::SCALEDFONTCREATION ||
        newEvent.recordedEvent->GetType() == RecordedEvent::SCALEDFONTDESTRUCTION) {
      fontCreations.push_back(newEvent);
    } else {
      drawingEvents.push_back(newEvent);
    }
  }

#ifdef WIN32
  RefPtr<ID3D10Device1> device;
  ::D3D10CreateDevice1(nullptr,
                       D3D10_DRIVER_TYPE_HARDWARE,
                       nullptr,
                       D3D10_CREATE_DEVICE_BGRA_SUPPORT |
                       D3D10_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS,
                       D3D10_FEATURE_LEVEL_10_0,
                       D3D10_1_SDK_VERSION,
                       byRef(device));

  Factory::SetDirect3D10Device(device);
#endif

  for (int i = 0; i < sizeof(sTestedBackends) / sizeof(BackendType); i++) {
    RefPtr<DrawTarget> dt = Factory::CreateDrawTarget(sTestedBackends[i], IntSize(1, 1), FORMAT_B8G8R8A8);

    RawTranslator* translator = new RawTranslator(dt);

    for (int c = 0; c < fontCreations.size(); c++) {
      translator->SetEventNumber(fontCreations[c].eventID);
      fontCreations[c].recordedEvent->PlayEvent(translator);
    }

    double data[sN + 1];
    double average = 0;

    for (int k = 0; k < (sN + 1); k++) {
      HighPrecisionMeasurement measurement;
      measurement.Start();
      for (int c = 0; c < drawingEvents.size(); c++) {
        translator->SetEventNumber(drawingEvents[c].eventID);
        drawingEvents[c].recordedEvent->PlayEvent(translator);
      }
      data[k] = measurement.Measure();
      if (k > 0) {
        average += data[k];
      }
    }
    average /= sN;

    double sqDiffSum = 0;
    for (int c = 1; c < sN + 1; c++) {
      sqDiffSum += pow(data[c] - average, 2);
    }

    sqDiffSum /= sN;

    printf("Rendering time (%s): %f +/- %f ms\n", GetBackendName(sTestedBackends[i]).c_str(), average, sqrt(sqDiffSum));

    delete translator;
  }

  return 0;
}
