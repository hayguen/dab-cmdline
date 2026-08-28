#include <atomic>
#include <chrono>
#include <complex>
#include <cstdlib>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <map>
#include <mutex>
#include <regex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>

#include "band-handler.h"
#include "charsets.h"
#include "dab_tables.h"
#include "dabsdr.h"
#include "device-handler.h"

#if defined(ABRA_INPUT_RTLSDR)
#include "rtlsdr-handler.h"
static constexpr bool kUseRtlSdrInput = true;
#else
#include "rawfiles.h"
static constexpr bool kUseRtlSdrInput = false;
#endif

struct ServiceInfo {
  uint8_t ptyS{0xFF};
  uint8_t ptyD{0xFF};
  uint8_t caId{0};
  std::string shortLabel;
};

struct AudioComponentInfo {
  uint8_t scids{0};
  uint8_t ascty{0};
  uint16_t bitrate{0};
  uint8_t protectionLevel{0};
  uint8_t uepIdx{0};
  uint8_t subchId{0};
  uint16_t subchSize{0};
  uint8_t lang{0};
  std::string label;
};

struct PacketComponentInfo {
  uint8_t scids{0};
  uint8_t subchId{0};
  uint8_t protectionLevel{0};
  uint8_t uepIdx{0};
  uint8_t dscty{0};
  uint16_t subchSize{0};
  uint16_t scid{0};
  uint8_t dgFlag{0};
};

struct AudioProbeInfo {
  uint32_t frames{0};
  int sbrFlag{-1};
  int psFlag{-1};
  int aacChannelMode{-1};
  int dacRateFlag{-1};
};

// A live scan commonly collects roughly 30 AAC headers per selected service.
// Sixteen consistent headers are sufficient to avoid a one-frame transient
// while still allowing live RTL scans to report decoded codec details.
static constexpr uint32_t kMinReliableAudioFrames = 16;

static bool probeHasReliableDetails(uint8_t ascty, const AudioProbeInfo &probe) {
  if (probe.frames == 0) {
    return false;
  }
  if (ascty == 0) {
    return true;
  }
  if (ascty == 63) {
    if (probe.frames < kMinReliableAudioFrames) {
      return false;
    }
    if (probe.sbrFlag < 0 || probe.aacChannelMode < 0 || probe.dacRateFlag < 0) {
      return false;
    }
    if (probe.sbrFlag == 1 && probe.psFlag < 0) {
      return false;
    }
    return true;
  }
  return false;
}

struct TiiInfo {
  int tii{0};
  int count{0};
  float maxLevel{-200.0F};
};

struct TiiStrengthContext {
  bool hasAny{false};
  bool looksLikeDbScale{false};
  bool allLevelsEqual{true};
  float maxLevel{0.0F};
  int maxCount{0};
};

struct AppState {
  std::mutex mtx;
  std::atomic<bool> gotSync{false};
  std::atomic<bool> gotServiceList{false};
  std::atomic<bool> requestedEnsembleAfterSync{false};
  std::atomic<bool> requestedServiceList{false};
  std::atomic<int> syncLevel{0};

  std::string inputFile;
  std::string channel;
  bool hasChannel{false};
  uint8_t band{BAND_III};
  int runSeconds{18};
  bool csvOutput{false};
  bool csvOnly{false};
  bool emitCsvService{false};
  bool emitCsvTii{false};
  bool emitAbraLines{true};
  int requestedSid{-1};
  int minSnrToExit{-32768};
  bool noRealtime{false};
  double fileOffsetSec{0.0};
  bool detailedAudio{false};
  bool rewindPerService{false};
  double rewindOffsetSec{0.0};
  int audioProbeMs{1800};
  std::string logPath;
  FILE *logFile{nullptr};
  bool rewindWarningPrinted{false};
  int ppmCorrection{0};
  int gain{100};
  bool autogain{false};
  int deviceIndex{0};
  std::string deviceSerial;
  std::string rtlOpts;

  dabsdrHandle_t handle{nullptr};
  deviceHandler *device{nullptr};
  std::vector<std::complex<float>> sampleBuf;

  uint32_t frequency{0};
  uint32_t ensembleUEID{0};
  uint16_t ensembleEID{0};
  uint8_t ensembleECC{0};
  uint8_t ensembleIntTable{0};
  std::string ensembleLabel;
  std::string ensembleShortLabel;
  std::map<uint32_t, std::string> services;
  std::map<uint32_t, ServiceInfo> serviceInfoBySid;
  std::map<uint32_t, std::vector<AudioComponentInfo>> audioComponents;
  std::map<uint32_t, std::vector<PacketComponentInfo>> packetComponents;
  std::unordered_map<uint64_t, uint16_t> userAppTypeByComp;
  std::unordered_map<uint64_t, AudioProbeInfo> audioProbe;
  std::unordered_map<uint64_t, bool> audioProbeSelectionOk;
  std::map<int, TiiInfo> tiiById;
  int snrMin{9999};
  int snrMax{-9999};
  int snrSum{0};
  int snrCount{0};
  int ficMin{101};
  int ficMax{-1};
  int ficSum{0};
  int ficCount{0};
  uint32_t activeProbeSid{0};
  uint8_t activeProbeScids{0};
  dabsdrDecoderId_t activeProbeDecoder{DABSDR_ID_AUDIO_PRIMARY};
  bool activeProbeSelectionSeen{false};
  bool activeProbeSelectionOk{false};
  uint32_t lastEnsembleLogUeid{0};
  std::unordered_set<uint32_t> loggedProgramSids;
  uint32_t audioCallbackCount{0};
  uint32_t audioCallbackCountDuringProbe{0};
  uint32_t audioCallbackPrimaryCount{0};
  uint32_t audioCallbackSecondaryCount{0};
  uint32_t tiiNotificationCount{0};
  uint32_t tiiNotificationNonZeroCount{0};
  uint16_t tiiLastLen{0};
  uint8_t tiiLastIdsRequested{0};
  uint32_t serviceSelNotificationCount{0};
  uint32_t serviceSelNotificationLenZeroCount{0};
  uint32_t serviceSelSuccessCount{0};
  uint32_t serviceSelNotReadyCount{0};
  uint32_t serviceSelOtherStatusCount{0};
  bool secondaryProbeEnabled{true};
  uint32_t secondaryProbeSuppressedCount{0};
  uint32_t inputCallbackCalls{0};
  uint32_t inputCallbackZeroReads{0};
  uint32_t inputSamplesRequested{0};
  uint32_t inputSamplesDelivered{0};
  uint32_t dummyCallbackCalls{0};
};

static AppState *gApp = nullptr;

static uint64_t makeSidScidsKey(uint32_t sid, uint8_t scids) {
  return (static_cast<uint64_t>(sid) << 8) | static_cast<uint64_t>(scids);
}

static void printToOutputs(FILE *primary, FILE *logFile, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(primary, fmt, ap);
  va_end(ap);

  if (logFile != nullptr) {
    va_list ap2;
    va_start(ap2, fmt);
    vfprintf(logFile, fmt, ap2);
    va_end(ap2);
    fflush(logFile);
  }
}

static uint8_t countryIdFromSid(uint32_t sid) {
  return (sid & 0xFFFF0000U) != 0U
             ? static_cast<uint8_t>((sid >> 20) & 0x0F)
             : static_cast<uint8_t>((sid >> 12) & 0x0F);
}

struct SidView {
  uint32_t displaySid{0};
  uint8_t effectiveEcc{0};
  uint8_t countryId{0};
};

static SidView resolveSidView(uint32_t sid, uint8_t ensembleEcc) {
  SidView v{};

  // Programme services may come as 24-bit values: [ECC:8][SId:16].
  // For dab-raw compatibility, display SId as 16-bit and use embedded ECC.
  if ((sid & 0xFF000000U) == 0U && (sid & 0x00FF0000U) != 0U) {
    v.displaySid = sid & 0xFFFFU;
    v.effectiveEcc = static_cast<uint8_t>((sid >> 16) & 0xFFU);
    v.countryId = static_cast<uint8_t>((v.displaySid >> 12) & 0x0FU);
    return v;
  }

  v.displaySid = sid;
  v.effectiveEcc = ensembleEcc;
  v.countryId = countryIdFromSid(sid);
  return v;
}

static uint8_t effectivePty(const ServiceInfo *svc) {
  if (svc == nullptr) {
    return 0xFF;
  }
  if (svc->ptyD != 0xFF) {
    return svc->ptyD;
  }
  return svc->ptyS;
}

static const char *channelModeText(const AudioProbeInfo *probe, uint8_t ascty) {
  if (probe == nullptr || probe->frames == 0) {
    return "unknown";
  }
  if (ascty == 63) {
    if (probe->psFlag == 1) {
      return "stereo";
    }
    return (probe->aacChannelMode == 1) ? "stereo" : "mono";
  }
  return "unknown";
}

static std::string csvQuote(const std::string &value) {
  std::string out;
  out.reserve(value.size() + 2);
  out.push_back('"');
  for (char c : value) {
    if (c == '"') {
      out.push_back('"');
      out.push_back('"');
    } else {
      out.push_back(c);
    }
  }
  out.push_back('"');
  return out;
}

static std::string lookupServiceLabel(const std::map<uint32_t, std::string> &services, uint32_t sid) {
  const auto it = services.find(sid);
  if (it == services.end()) {
    return "";
  }
  return it->second;
}

static uint8_t inferEnsembleEccFromServices(const std::map<uint32_t, std::string> &services) {
  for (const auto &kv : services) {
    const uint32_t sid = kv.first;
    // Data services often carry ECC in the top byte of 32-bit SId.
    if ((sid & 0xFF000000U) != 0U) {
      const uint8_t ecc = static_cast<uint8_t>((sid >> 24) & 0xFFU);
      if (ecc != 0U) {
        return ecc;
      }
    }
    // Programme services may be represented as 24-bit [ECC:8][SId:16].
    if ((sid & 0xFF000000U) == 0U && (sid & 0x00FF0000U) != 0U) {
      const uint8_t ecc = static_cast<uint8_t>((sid >> 16) & 0xFFU);
      if (ecc != 0U) {
        return ecc;
      }
    }
  }
  return 0U;
}

static std::string toHex(uint32_t value) {
  char buf[32]{};
  std::snprintf(buf, sizeof(buf), "0x%x", static_cast<unsigned>(value));
  return std::string(buf);
}

static TiiStrengthContext buildTiiStrengthContext(const std::map<int, TiiInfo> &tiiById) {
  TiiStrengthContext ctx{};
  float minLevel = 0.0F;

  for (const auto &kv : tiiById) {
    const TiiInfo &t = kv.second;
    if (t.tii <= 0 || t.count <= 0 || !std::isfinite(t.maxLevel)) {
      continue;
    }

    if (!ctx.hasAny) {
      ctx.hasAny = true;
      ctx.maxLevel = t.maxLevel;
      minLevel = t.maxLevel;
      ctx.maxCount = t.count;
    } else {
      if (t.maxLevel > ctx.maxLevel) {
        ctx.maxLevel = t.maxLevel;
      }
      if (t.maxLevel < minLevel) {
        minLevel = t.maxLevel;
      }
      if (t.count > ctx.maxCount) {
        ctx.maxCount = t.count;
      }
    }
  }

  if (!ctx.hasAny) {
    return ctx;
  }

  // Heuristic: dabsdr TII levels are often in dB-like range [-200..200].
  ctx.looksLikeDbScale = (minLevel < 0.0F) || (ctx.maxLevel > 20.0F);
  ctx.allLevelsEqual = std::fabs(ctx.maxLevel - minLevel) < 1e-4F;
  return ctx;
}

static float computeTiiStrengthPct(const TiiInfo &t, const TiiStrengthContext &ctx) {
  if (!ctx.hasAny || t.count <= 0) {
    return 0.0F;
  }

  float strength = 0.0F;
  if (!ctx.allLevelsEqual) {
    if (ctx.looksLikeDbScale) {
      const float ratio = std::pow(10.0F, (t.maxLevel - ctx.maxLevel) / 10.0F);
      strength = 100.0F * ratio;
    } else if (ctx.maxLevel > 0.0F) {
      strength = 100.0F * (t.maxLevel / ctx.maxLevel);
    }
  }

  // Fallback when all reported levels are identical or unusable.
  if (!(strength > 0.0F) || !std::isfinite(strength)) {
    if (ctx.maxCount > 0) {
      strength = 100.0F * (static_cast<float>(t.count) / static_cast<float>(ctx.maxCount));
    } else {
      strength = 0.0F;
    }
  }

  if (strength < 0.0F) {
    strength = 0.0F;
  } else if (strength > 100.0F) {
    strength = 100.0F;
  }
  return strength;
}

static const char *normalizeUserAppText(uint16_t appType) {
  if (appType == 7) {
    // Match dab-raw wording exactly.
    return "SPI; was EPG";
  }
  return getUserApplicationType(static_cast<int16_t>(appType));
}

static std::string codecDescription(uint8_t ascty, const AudioProbeInfo *probe, bool selectionOk) {
  (void)selectionOk;
  if (ascty == 63) {
    if (probe == nullptr || probe->frames == 0) {
      return "DAB+/too weak or no audio";
    }
    // With too few frames available, mark as unresolved.
    if (probe->frames < kMinReliableAudioFrames) {
      return "DAB+/too weak or no audio";
    }
    // Enough frames decoded, but codec parameters not reliably detected (flags still -1).
    // This can happen with weak signals where frame data is corrupted.
    if (probe->sbrFlag < 0 || probe->aacChannelMode < 0) {
      return "DAB+/too weak or no audio";
    }
    if (probe->sbrFlag == 1 && probe->psFlag == 1) {
      return "DAB+/HE-AAC v2 Parametric Stereo";
    }
    if (probe->sbrFlag == 1) {
      return (probe->aacChannelMode == 1) ? "DAB+/HE-AAC v1 Stereo" : "DAB+/HE-AAC v1 Mono";
    }
    return (probe->aacChannelMode == 1) ? "DAB+/AAC-LC Stereo" : "DAB+/AAC-LC Mono";
  }
  if (ascty == 0) {
    if (probe != nullptr && probe->frames > 0) {
      return "DAB/MP2";
    }
    return "DAB/too weak or no audio";
  }
  return "unknown";
}

static std::string codecDescriptionWithRate(uint8_t ascty, const AudioProbeInfo *probe, bool selectionOk) {
  std::string codec = codecDescription(ascty, probe, selectionOk);
  if (probe == nullptr || probe->frames < kMinReliableAudioFrames) {
    return codec;
  }
  if (ascty == 63) {
    if (probe->dacRateFlag == 1) {
      codec += " 48kHz";
    } else if (probe->dacRateFlag == 0) {
      codec += " 32kHz";
    }
  }
  if (codec == "DAB+/AAC-LC Mono 32kHz") {
    return "DAB+/too weak or no audio";
  }
  return codec;
}

static std::string codecDescriptionCsvScanFallback(uint8_t ascty) {
  if (ascty == 63) {
    return "DAB+/audio";
  }
  if (ascty == 0) {
    return "DAB/MP2";
  }
  return "unknown";
}

static size_t rawLabelLength(const dabsdrLabel_t &label) {
  size_t n = 0;
  while (n < DAB_LABEL_MAX_LENGTH && label.str[n] != '\0') {
    ++n;
  }
  return n;
}

static std::string decodeLabelChunk(const char *bytes, size_t len, uint8_t charset) {
  if (bytes == nullptr || len == 0) {
    return "";
  }

  std::vector<char> tmp(len + 1, '\0');
  for (size_t i = 0; i < len; ++i) {
    tmp[i] = bytes[i];
  }

  CharacterSet cs = static_cast<CharacterSet>(charset);
  if (charset <= 0x03) {
    // ETSI: charsets 0..3 are EBU Latin-based repertoires.
    cs = EbuLatin;
  }
  return toStringUsingCharset(tmp.data(), cs, static_cast<int>(len));
}

static std::string labelToString(const dabsdrLabel_t &label) {
  return decodeLabelChunk(label.str, rawLabelLength(label), label.charset);
}

static std::string trimRight(const std::string &s) {
  size_t end = s.size();
  while (end > 0 && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r' || s[end - 1] == '\n')) {
    --end;
  }
  return s.substr(0, end);
}

static std::string deriveShortLabel(const dabsdrLabel_t &label) {
  const size_t n = rawLabelLength(label);
  if (n == 0) {
    return "";
  }

  auto fromMask = [&](bool msbFirst) -> std::string {
    std::vector<char> picked;
    picked.reserve(n);
    for (size_t i = 0; i < n; ++i) {
      const uint16_t bit = msbFirst ? static_cast<uint16_t>(1u << (15 - i))
                                    : static_cast<uint16_t>(1u << i);
      if ((label.charField & bit) != 0) {
        picked.push_back(label.str[i]);
      }
    }

    while (!picked.empty() && picked.back() == ' ') {
      picked.pop_back();
    }

    if (picked.empty()) {
      return "";
    }
    return trimRight(decodeLabelChunk(picked.data(), picked.size(), label.charset));
  };

  if (label.charField != 0) {
    std::string out = fromMask(true);
    if (!out.empty()) {
      return out;
    }
    out = fromMask(false);
    if (!out.empty()) {
      return out;
    }
  }

  const size_t fallbackLen = (n > 8U) ? 8U : n;
  return trimRight(decodeLabelChunk(label.str, fallbackLen, label.charset));
}

static bool chooseNearestEepLevel(uint16_t bitrate, uint16_t subchSize,
                                  uint8_t rawProtectionLevel, uint8_t uepIdx,
                                  int16_t *protLevel, double *bestDiffOut) {
  if (protLevel == nullptr || bitrate == 0 || subchSize == 0) {
    return false;
  }

  struct Candidate {
    int16_t level;
    double factor;
  };

  // EEP-A (0..3), EEP-B (4..7)
  // A factors (SubChSize/bitrate): 12/8=1.5, 8/8=1.0, 6/8=0.75, 4/8=0.5
  // B factors (SubChSize/bitrate): 27/32, 21/32, 18/32, 15/32
  const Candidate candidates[] = {
      {0, 1.5}, {1, 1.0}, {2, 0.75}, {3, 0.5},
      {4, 27.0/32}, {5, 21.0/32}, {6, 18.0/32}, {7, 15.0/32}};

  int bestIdx = -1;
  double bestDiff = 1e9;
  for (int i = 0; i < static_cast<int>(sizeof(candidates) / sizeof(candidates[0])); ++i) {
    const double expected = candidates[i].factor * static_cast<double>(bitrate);
    const double diff = std::fabs(expected - static_cast<double>(subchSize));
    if (diff < bestDiff) {
      bestDiff = diff;
      bestIdx = i;
    }
  }

  if (bestIdx < 0) {
    return false;
  }

  *protLevel = candidates[bestIdx].level;
  if (bestDiffOut != nullptr) {
    *bestDiffOut = bestDiff;
  }
  return true;
}

static bool inferEepLevelFromSubch(uint16_t bitrate, uint16_t subchSize,
                                   uint8_t rawProtectionLevel, uint8_t uepIdx,
                                   int16_t *protLevel) {
  double bestDiff = 0.0;
  int16_t nearest = 0;
  if (!chooseNearestEepLevel(bitrate, subchSize, rawProtectionLevel, uepIdx,
                             &nearest, &bestDiff)) {
    return false;
  }

  // Allow absolute and relative tolerance for broadcaster-specific rounding.
  const double relTol = 0.08 * static_cast<double>(bitrate);
  if (bestDiff <= 3.0 || bestDiff <= relTol) {
    *protLevel = nearest;
    return true;
  }
  return false;
}

static uint16_t estimateBitrateFromProtection(bool shortForm, int16_t protLevel, uint16_t subchSize) {
  if (subchSize == 0 || shortForm) {
    return 0;
  }

  double factor = 0.0;
  switch (protLevel) {
    case 0: factor = 1.5; break;   // EEP 1-A
    case 1: factor = 1.0; break;   // EEP 2-A
    case 2: factor = 0.75; break;  // EEP 3-A
    case 3: factor = 0.5; break;   // EEP 4-A
    case 4: factor = 27.0/32; break;  // EEP 1-B
    case 5: factor = 21.0/32; break;  // EEP 2-B
    case 6: factor = 18.0/32; break;  // EEP 3-B
    case 7: factor = 15.0/32; break;  // EEP 4-B
    default: return 0;
  }

  if (factor <= 0.0) {
    return 0;
  }

  const double bitrate = static_cast<double>(subchSize) / factor;
  if (bitrate < 1.0) {
    return 0;
  }
  return static_cast<uint16_t>(bitrate + 0.5);
}

static void decodeProtection(uint8_t rawProtectionLevel, uint8_t uepIdx,
                             uint16_t bitrate, uint16_t subchSize,
                             bool *shortForm, int16_t *protLevel) {
  if (shortForm == nullptr || protLevel == nullptr) {
    return;
  }

  // libdabsdr v4 DabProtectionLevel encoding:
  //   0      = undefined
  //   1..5   = UEP 1..5
  //   6..9   = EEP 1-A..4-A  (internal protLevel = raw - 6 => 0..3)
  //   10..13 = EEP 1-B..4-B  (internal protLevel = raw - 6 => 4..7)
  if (rawProtectionLevel >= 6 && rawProtectionLevel <= 13) {
    *shortForm = false;
    *protLevel = static_cast<int16_t>(rawProtectionLevel - 6);
    return;
  }

  if (rawProtectionLevel >= 1 && rawProtectionLevel <= 5) {
    *shortForm = true;
    *protLevel = static_cast<int16_t>(rawProtectionLevel);
    return;
  }

  // rawProtectionLevel == 0: infer from bitrate/subchannel geometry.
  int16_t inferredEep = 0;
  if (inferEepLevelFromSubch(bitrate, subchSize, rawProtectionLevel, uepIdx, &inferredEep)) {
    *shortForm = false;
    *protLevel = inferredEep;
    return;
  }

  // Hard fallback.
  int16_t nearestEep = 0;
  if (chooseNearestEepLevel(bitrate, subchSize, rawProtectionLevel, uepIdx,
                            &nearestEep, nullptr)) {
    *shortForm = false;
    *protLevel = nearestEep;
    return;
  }

  *shortForm = false;
  *protLevel = 0;
}

static bool detectChannelFromFilename(const std::string &path, std::string *outChannel) {
  if (outChannel == nullptr) {
    return false;
  }

  const std::regex channelRx("(^|[^A-Za-z0-9])([0-9]{1,2}[A-Da-d])([^A-Za-z0-9]|$)");
  std::smatch m;
  if (!std::regex_search(path, m, channelRx) || m.size() < 3) {
    return false;
  }

  std::string ch = m[2].str();
  for (char &c : ch) {
    if (c >= 'a' && c <= 'z') {
      c = static_cast<char>(c - ('a' - 'A'));
    }
  }

  *outChannel = ch;
  return true;
}

static bool parseHexSid(const std::string &txt, int *outSid) {
  if (outSid == nullptr) {
    return false;
  }
  char *end = nullptr;
  const long v = std::strtol(txt.c_str(), &end, 16);
  if (end == txt.c_str() || *end != '\0' || v < 0 || v > 0xFFFFFFFFL) {
    return false;
  }
  *outSid = static_cast<int>(v);
  return true;
}

static void inputCallback(float out[], uint16_t len) {
  if (gApp == nullptr || gApp->device == nullptr || out == nullptr || len == 0) {
    return;
  }

  {
    std::lock_guard<std::mutex> lk(gApp->mtx);
    gApp->inputCallbackCalls += 1;
    gApp->inputSamplesRequested += static_cast<uint32_t>(len);
  }

  const int32_t numComplex = static_cast<int32_t>(len);
  if (numComplex <= 0) {
    return;
  }

  if (static_cast<int32_t>(gApp->sampleBuf.size()) < numComplex) {
    gApp->sampleBuf.resize(numComplex);
  }

  const int32_t got = gApp->device->getSamples(gApp->sampleBuf.data(), numComplex);

  {
    std::lock_guard<std::mutex> lk(gApp->mtx);
    if (got <= 0) {
      gApp->inputCallbackZeroReads += 1;
    } else {
      gApp->inputSamplesDelivered += static_cast<uint32_t>(got);
    }
  }

  int32_t i = 0;
  for (; i < got; ++i) {
    out[2 * i] = gApp->sampleBuf[i].real();
    out[2 * i + 1] = gApp->sampleBuf[i].imag();
  }
  for (; i < numComplex; ++i) {
    out[2 * i] = 0.0F;
    out[2 * i + 1] = 0.0F;
  }
}

static void dummyInputCallback(float out[], uint16_t len) {
  if (gApp != nullptr) {
    std::lock_guard<std::mutex> lk(gApp->mtx);
    gApp->dummyCallbackCalls += 1;
  }
  // This libdabsdr build actively pulls IQ through the dummy path during
  // live decoding, so it must receive the same device feed as the main path.
  inputCallback(out, len);
}

static void audioCallback(dabsdrAudioCBData_t *p, void *ctx) {
  if (p == nullptr || ctx == nullptr) {
    return;
  }

  auto *app = static_cast<AppState *>(ctx);

  std::lock_guard<std::mutex> lk(app->mtx);
  app->audioCallbackCount += 1;
  if (p->id == DABSDR_ID_AUDIO_PRIMARY) {
    app->audioCallbackPrimaryCount += 1;
  } else if (p->id == DABSDR_ID_AUDIO_SECONDARY) {
    app->audioCallbackSecondaryCount += 1;
  }
  if (app->activeProbeSid == 0) {
    return;
  }

  const uint64_t key = makeSidScidsKey(app->activeProbeSid, app->activeProbeScids);
  auto &probe = app->audioProbe[key];
  probe.frames += 1;
  app->audioCallbackCountDuringProbe += 1;
  probe.sbrFlag = p->header.bits.sbr_flag;
  probe.psFlag = p->header.bits.ps_flag;
  probe.aacChannelMode = p->header.bits.aac_channel_mode;
  probe.dacRateFlag = p->header.bits.dac_rate;
}

static void probeAudioDetails(AppState &app) {
  if (!app.detailedAudio) {
    return;
  }

  const bool csvMultiService = app.csvOutput && app.requestedSid == -1;
  const bool explicitAbraDetailRequest = app.emitAbraLines && !app.csvOnly;
  const bool explicitDetailedAudioRequest = app.detailedAudio;

  // Keep dab-rtlsdr CSV scan semantics by default, but if the caller
  // explicitly asks for ABRA_* lines or -D/--audio-details, allow deep
  // probing for detailed codec metadata.
  if (csvMultiService && !explicitAbraDetailRequest && !explicitDetailedAudioRequest) {
    return;
  }

  std::map<uint32_t, std::vector<AudioComponentInfo>> comps;
  {
    std::lock_guard<std::mutex> lk(app.mtx);
    comps = app.audioComponents;
  }

  if (app.requestedSid != -1) {
    // Stop any implicit/default primary selections first so target SID can
    // be selected cleanly for probing on live input.
    for (const auto &svc : comps) {
      for (const auto &comp : svc.second) {
        dabsdrRequest_ServiceStop(app.handle, svc.first, comp.scids, DABSDR_ID_AUDIO_PRIMARY);
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
  }

  for (const auto &svc : comps) {
    const uint32_t sid = svc.first;
    if (app.requestedSid != -1 && static_cast<int>(sid) != app.requestedSid) {
      continue;
    }
    for (const auto &comp : svc.second) {
      if (comp.ascty != 0 && comp.ascty != 63) {
        continue;
      }

      if (app.rewindPerService && app.device != nullptr) {
        const bool rewound = app.device->seekToSeconds(app.rewindOffsetSec) || app.device->rewindToStart();
        if (!rewound && !app.rewindWarningPrinted) {
          printToOutputs(stderr, app.logFile,
                         "warning: --rewind requested but input cannot rewind; continuing without rewind\n");
          app.rewindWarningPrinted = true;
        }
      }

      {
        std::lock_guard<std::mutex> lk(app.mtx);
        app.activeProbeSid = sid;
        app.activeProbeScids = comp.scids;
        app.activeProbeDecoder = DABSDR_ID_AUDIO_PRIMARY;
        app.activeProbeSelectionSeen = false;
        app.activeProbeSelectionOk = false;
        app.audioProbe[makeSidScidsKey(sid, comp.scids)] = AudioProbeInfo{};
        app.audioProbeSelectionOk[makeSidScidsKey(sid, comp.scids)] = false;
      }

      const uint64_t probeKey = makeSidScidsKey(sid, comp.scids);

      if (app.requestedSid != -1) {
        dabsdrRequest_ServiceStop(app.handle, sid, comp.scids, DABSDR_ID_AUDIO_PRIMARY);
        dabsdrRequest_ServiceStop(app.handle, sid, comp.scids, DABSDR_ID_AUDIO_SECONDARY);

        {
          std::lock_guard<std::mutex> lk(app.mtx);
          app.activeProbeDecoder = DABSDR_ID_AUDIO_PRIMARY;
          app.activeProbeSelectionSeen = false;
          app.activeProbeSelectionOk = false;
        }

        const auto tSelectEnd = std::chrono::steady_clock::now() + std::chrono::milliseconds(4200);
        auto nextSelectRetry = std::chrono::steady_clock::now();
        bool selectionOk = false;
        while (std::chrono::steady_clock::now() < tSelectEnd) {
          const auto now = std::chrono::steady_clock::now();
          if (now >= nextSelectRetry) {
            dabsdrRequest_ServiceSelection(app.handle, sid, comp.scids, DABSDR_ID_AUDIO_PRIMARY);
            nextSelectRetry = now + std::chrono::milliseconds(480);
          }

          {
            std::lock_guard<std::mutex> lk(app.mtx);
            const auto pit = app.audioProbe.find(probeKey);
            if (pit != app.audioProbe.end() && pit->second.frames > 0) {
              selectionOk = true;
              break;
            }
            if (app.activeProbeSelectionSeen) {
              if (app.activeProbeSelectionOk) {
                selectionOk = true;
                break;
              }
              app.activeProbeSelectionSeen = false;
            }
          }

          std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        if (selectionOk) {
          const int probeWindowMs = std::max(app.audioProbeMs, 5500);
          const auto tProbeEnd = std::chrono::steady_clock::now() + std::chrono::milliseconds(probeWindowMs);
          auto nextKeepaliveSelect = std::chrono::steady_clock::now() + std::chrono::milliseconds(1100);
          while (std::chrono::steady_clock::now() < tProbeEnd) {
            const auto now = std::chrono::steady_clock::now();
            if (now >= nextKeepaliveSelect) {
              dabsdrRequest_ServiceSelection(app.handle, sid, comp.scids, DABSDR_ID_AUDIO_PRIMARY);
              nextKeepaliveSelect = now + std::chrono::milliseconds(1100);
            }

            {
              std::lock_guard<std::mutex> lk(app.mtx);
              const auto pit = app.audioProbe.find(probeKey);
              if (pit != app.audioProbe.end() && pit->second.frames >= kMinReliableAudioFrames) {
                break;
              }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(20));
          }
        }

        dabsdrRequest_ServiceStop(app.handle, sid, comp.scids, DABSDR_ID_AUDIO_PRIMARY);
        continue;
      }

      const int maxProbeAttempts = csvMultiService ? 2 : 1;
      for (int probeAttempt = 0; probeAttempt < maxProbeAttempts; ++probeAttempt) {
        bool trySecondary = false;
        {
          std::lock_guard<std::mutex> lk(app.mtx);
          trySecondary = app.secondaryProbeEnabled;
        }
        const dabsdrDecoderId_t decoderOrder[2] = {
            DABSDR_ID_AUDIO_PRIMARY,
            DABSDR_ID_AUDIO_SECONDARY,
        };
        const int decoderCount = trySecondary ? 2 : 1;

        bool gotFramesThisAttempt = false;
        for (int decoderIdx = 0; decoderIdx < decoderCount; ++decoderIdx) {
          const dabsdrDecoderId_t decoderId = decoderOrder[decoderIdx];
          dabsdrRequest_ServiceStop(app.handle, sid, comp.scids, DABSDR_ID_AUDIO_PRIMARY);
          dabsdrRequest_ServiceStop(app.handle, sid, comp.scids, DABSDR_ID_AUDIO_SECONDARY);

          {
            std::lock_guard<std::mutex> lk(app.mtx);
            app.activeProbeDecoder = decoderId;
            app.activeProbeSelectionSeen = false;
            app.activeProbeSelectionOk = false;
          }

          const int selectTimeoutMs = (app.requestedSid != -1) ? 2800 : 1400;
          const auto tSelectEnd = std::chrono::steady_clock::now() + std::chrono::milliseconds(selectTimeoutMs);
          auto nextSelectRetry = std::chrono::steady_clock::now();
          bool selectionOk = false;
          bool sawSelectionResponse = false;
          while (std::chrono::steady_clock::now() < tSelectEnd) {
            const auto now = std::chrono::steady_clock::now();
            if (now >= nextSelectRetry) {
              dabsdrRequest_ServiceSelection(app.handle, sid, comp.scids, decoderId);
              nextSelectRetry = now + std::chrono::milliseconds(520);
            }

            {
              std::lock_guard<std::mutex> lk(app.mtx);
              const auto pit = app.audioProbe.find(probeKey);
              if (pit != app.audioProbe.end() && pit->second.frames > 0) {
                selectionOk = true;
                break;
              }
              if (app.activeProbeSelectionSeen) {
                sawSelectionResponse = true;
                if (app.activeProbeSelectionOk) {
                  selectionOk = true;
                  break;
                }
                // Decoder may report temporary not-ready states; keep retrying.
                app.activeProbeSelectionSeen = false;
              }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(20));
          }

          if (!selectionOk && !sawSelectionResponse) {
            dabsdrRequest_ServiceSelection(app.handle, sid, comp.scids, decoderId);
            std::this_thread::sleep_for(std::chrono::milliseconds(250));

            std::lock_guard<std::mutex> lk(app.mtx);
            const auto pit = app.audioProbe.find(probeKey);
            if (pit != app.audioProbe.end() && pit->second.frames > 0) {
              selectionOk = true;
            }
          }

          if (!selectionOk) {
            dabsdrRequest_ServiceStop(app.handle, sid, comp.scids, decoderId);
            continue;
          }

          const int probeWindowMs = (app.requestedSid != -1)
                      ? std::max(app.audioProbeMs, 2800)
                      : app.audioProbeMs;
          const auto tEnd = std::chrono::steady_clock::now() + std::chrono::milliseconds(probeWindowMs);
          bool gotReliableDetails = false;
          while (std::chrono::steady_clock::now() < tEnd) {
            {
              std::lock_guard<std::mutex> lk(app.mtx);
              const auto pit = app.audioProbe.find(probeKey);
              if (pit != app.audioProbe.end() && probeHasReliableDetails(comp.ascty, pit->second)) {
                gotReliableDetails = true;
                break;
              }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
          }

          dabsdrRequest_ServiceStop(app.handle, sid, comp.scids, decoderId);

          bool gotFrames = false;
          {
            std::lock_guard<std::mutex> lk(app.mtx);
            const auto pit = app.audioProbe.find(probeKey);
            gotFrames = (pit != app.audioProbe.end() && pit->second.frames > 0);
          }
          if (gotReliableDetails || gotFrames) {
            gotFramesThisAttempt = true;
            break;
          }

          std::this_thread::sleep_for(std::chrono::milliseconds(90));
        }

        if (gotFramesThisAttempt) {
          break;
        }
      }
    }
  }

  std::lock_guard<std::mutex> lk(app.mtx);
  app.activeProbeSid = 0;
  app.activeProbeScids = 0;
  app.activeProbeDecoder = DABSDR_ID_AUDIO_PRIMARY;
  app.activeProbeSelectionSeen = false;
  app.activeProbeSelectionOk = false;
}

static void notificationCallback(dabsdrNotificationCBData_t *p, void *ctx) {
  if (p == nullptr || ctx == nullptr) {
    return;
  }

  auto *app = static_cast<AppState *>(ctx);

  switch (p->nid) {
    case DABSDR_NID_SYNC_STATUS: {
      if (p->len < sizeof(dabsdrNtfSyncStatus_t) || p->pData == nullptr) {
        return;
      }
      const auto *sync = static_cast<const dabsdrNtfSyncStatus_t *>(p->pData);

      {
        std::lock_guard<std::mutex> lk(app->mtx);
        const int snr = static_cast<int>(sync->snr10 / 10);
        if (snr < app->snrMin) {
          app->snrMin = snr;
        }
        if (snr > app->snrMax) {
          app->snrMax = snr;
        }
        app->snrSum += snr;
        app->snrCount += 1;
      }

      app->syncLevel.store(static_cast<int>(sync->syncLevel));
      if (sync->syncLevel >= DABSDR_SYNC_LEVEL_FIC) {
        app->gotSync.store(true);
        if (!app->requestedEnsembleAfterSync.exchange(true)) {
          dabsdrRequest_GetEnsemble(app->handle);
        }
      }
      break;
    }
    case DABSDR_NID_PERIODIC: {
      if (p->len < sizeof(dabsdrNtfPeriodic_t) || p->pData == nullptr) {
        return;
      }
      const auto *per = static_cast<const dabsdrNtfPeriodic_t *>(p->pData);

      // Approximate FIC quality [%] from periodic FIB CRC errors.
      // Zero errors => 100, increasing errors lower the score and clamp at 0.
      int fic = 100 - static_cast<int>(per->fibErrorCntr);
      if (fic < 0) {
        fic = 0;
      }
      if (fic > 100) {
        fic = 100;
      }

      std::lock_guard<std::mutex> lk(app->mtx);
      if (fic < app->ficMin) {
        app->ficMin = fic;
      }
      if (fic > app->ficMax) {
        app->ficMax = fic;
      }
      app->ficSum += fic;
      app->ficCount += 1;
      break;
    }
    case DABSDR_NID_ENSEMBLE_INFO: {
      if (p->len < sizeof(dabsdrNtfEnsemble_t) || p->pData == nullptr) {
        return;
      }
      const auto *e = static_cast<const dabsdrNtfEnsemble_t *>(p->pData);
      const uint16_t eidFromUeid = static_cast<uint16_t>(e->ueid & 0xFFFFU);
      const uint8_t eccFromUeid = static_cast<uint8_t>((e->ueid >> 16) & 0xFFU);
      const uint16_t resolvedEid = (eidFromUeid != 0U) ? eidFromUeid : e->eid;
      const uint8_t resolvedEcc = (eccFromUeid != 0U) ? eccFromUeid : e->ecc;
      {
        std::lock_guard<std::mutex> lk(app->mtx);
        app->ensembleUEID = e->ueid;
        app->ensembleEID = resolvedEid;
        app->ensembleECC = resolvedEcc;
        app->ensembleIntTable = e->intTable;
        app->ensembleLabel = labelToString(e->label);
        app->ensembleShortLabel = deriveShortLabel(e->label);
      }
      // Emit dab-raw compatible ensemblenameHandler line so that shell scripts
      // (e.g. scanDAB.sh) can parse the 16-char padded ensemble name and EId.
      // Guard: only emit when the label is non-empty and EId is valid.
      if (resolvedEid != 0U && e->label.str[0] != '\0') {
        std::lock_guard<std::mutex> lk(app->mtx);
        if (!app->ensembleLabel.empty() && app->lastEnsembleLogUeid != e->ueid) {
          app->lastEnsembleLogUeid = e->ueid;
          fprintf(stderr, "\nensemblenameHandler: '%s' / '%s' ensemble (EId %X) is recognized\n\n",
                  app->ensembleLabel.c_str(), app->ensembleShortLabel.c_str(),
                  static_cast<unsigned>(resolvedEid));
        }
      }
      if (!app->requestedServiceList.exchange(true)) {
        dabsdrRequest_GetServiceList(app->handle);
      }
      break;
    }
    case DABSDR_NID_SERVICE_LIST: {
      if (p->len < sizeof(dabsdrNtfServiceList_t) || p->pData == nullptr) {
        return;
      }
      const auto *sl = static_cast<const dabsdrNtfServiceList_t *>(p->pData);
      std::map<uint32_t, std::string> tmp;

      for (uint8_t i = 0; i < sl->numServices; ++i) {
        dabsdrServiceListItem_t item{};
        if (sl->getServiceListItem == nullptr) {
          break;
        }
        if (sl->getServiceListItem(app->handle, i, &item) == 0) {
          const std::string serviceLabel = labelToString(item.label);
          const std::string serviceShortLabel = deriveShortLabel(item.label);
          tmp[item.sid] = serviceLabel;
          bool shouldLogProgram = false;
          {
            std::lock_guard<std::mutex> lk(app->mtx);
            auto &serviceInfo = app->serviceInfoBySid[item.sid];
            if (item.pty.s != 0xFF) {
              serviceInfo.ptyS = item.pty.s;
            }
            if (item.pty.d != 0xFF) {
              serviceInfo.ptyD = item.pty.d;
            }
            serviceInfo.caId = item.CAId;
            serviceInfo.shortLabel = serviceShortLabel;
            shouldLogProgram = app->loggedProgramSids.insert(item.sid).second;
          }
          if (shouldLogProgram) {
            fprintf(stderr,
                    "programnameHandler: '%s' / '%s' (SId %X) is part of the ensemble\n",
                    serviceLabel.c_str(), serviceShortLabel.c_str(),
                    static_cast<unsigned>(item.sid));
          }
          dabsdrRequest_GetServiceComponents(app->handle, item.sid);
        }
      }

      {
        std::lock_guard<std::mutex> lk(app->mtx);
        app->services.swap(tmp);
      }
      app->gotServiceList.store(true);
      break;
    }
    case DABSDR_NID_SERVICE_COMPONENT_LIST: {
      if (p->len < sizeof(dabsdrNtfServiceComponentList_t) || p->pData == nullptr) {
        return;
      }
      const auto *cl = static_cast<const dabsdrNtfServiceComponentList_t *>(p->pData);
      if (cl->getServiceComponentListItem == nullptr) {
        return;
      }

      std::vector<AudioComponentInfo> comps;
      std::vector<PacketComponentInfo> packetComps;
      for (uint8_t i = 0; i < cl->numServiceComponents; ++i) {
        dabsdrServiceCompListItem_t item{};
        if (cl->getServiceComponentListItem(app->handle, i, &item) != 0) {
          continue;
        }
        if (item.TMId == 0) {
          AudioComponentInfo info{};
          info.scids = item.SCIdS;
          info.ascty = item.streamAudio.ASCTy;
          info.bitrate = item.streamAudio.bitRate;
          info.protectionLevel = item.protectionLevel;
          info.uepIdx = item.uepIdx;
          info.subchId = item.SubChId;
          info.subchSize = item.SubChSize;
          info.lang = item.lang;
          info.label = labelToString(item.label);
          comps.push_back(info);
        } else if (item.TMId == 3) {
          PacketComponentInfo pi{};
          pi.scids = item.SCIdS;
          pi.subchId = item.SubChId;
          pi.protectionLevel = item.protectionLevel;
          pi.uepIdx = item.uepIdx;
          pi.dscty = item.packetData.DSCTy;
          pi.subchSize = item.SubChSize;
          pi.scid = item.packetData.SCId;
          pi.dgFlag = item.packetData.DGflag;
          packetComps.push_back(pi);
          dabsdrRequest_GetUserAppList(app->handle, cl->SId, item.SCIdS);
        }
      }

      {
        std::lock_guard<std::mutex> lk(app->mtx);
        app->audioComponents[cl->SId] = comps;
        app->packetComponents[cl->SId] = packetComps;
      }
      break;
    }
    case DABSDR_NID_SERVICE_SELECTION: {
      if (p->pData == nullptr) {
        return;
      }

      if (p->len < sizeof(dabsdrNtfServiceSelection_t)) {
        std::lock_guard<std::mutex> lk(app->mtx);
        app->serviceSelNotificationCount += 1;
        if (p->len == 0) {
          app->serviceSelNotificationLenZeroCount += 1;
        }

        if (app->activeProbeSid != 0) {
          app->activeProbeSelectionSeen = true;
          app->activeProbeSelectionOk = (p->status == DABSDR_NSTAT_SUCCESS);
        }

        if (p->status == DABSDR_NSTAT_SUCCESS) {
          app->serviceSelSuccessCount += 1;
          if (app->activeProbeSid != 0) {
            app->audioProbeSelectionOk[makeSidScidsKey(app->activeProbeSid, app->activeProbeScids)] = true;
          }
        } else if (p->status == DABSDR_NSTAT_SERVICE_NOT_READY) {
          app->serviceSelNotReadyCount += 1;
        } else {
          app->serviceSelOtherStatusCount += 1;
          if (app->activeProbeSid != 0 &&
              app->activeProbeDecoder == DABSDR_ID_AUDIO_SECONDARY &&
              app->secondaryProbeEnabled) {
            app->secondaryProbeEnabled = false;
            app->secondaryProbeSuppressedCount += 1;
          }
        }
        return;
      }

      const auto *sel = static_cast<const dabsdrNtfServiceSelection_t *>(p->pData);

      std::lock_guard<std::mutex> lk(app->mtx);
      app->serviceSelNotificationCount += 1;
      if (app->activeProbeSid == sel->SId &&
          app->activeProbeScids == sel->SCIdS &&
          app->activeProbeDecoder == sel->id) {
        app->activeProbeSelectionSeen = true;
        app->activeProbeSelectionOk = (p->status == DABSDR_NSTAT_SUCCESS);
      }

      if (p->status == DABSDR_NSTAT_SUCCESS) {
        app->serviceSelSuccessCount += 1;
        if (app->activeProbeSid == sel->SId &&
            app->activeProbeScids == sel->SCIdS &&
            app->activeProbeDecoder == sel->id) {
          app->audioProbeSelectionOk[makeSidScidsKey(sel->SId, sel->SCIdS)] = true;
        }
      } else if (p->status == DABSDR_NSTAT_SERVICE_NOT_READY) {
        app->serviceSelNotReadyCount += 1;
      } else {
        app->serviceSelOtherStatusCount += 1;
        if (app->activeProbeSid == sel->SId &&
            app->activeProbeScids == sel->SCIdS &&
            app->activeProbeDecoder == DABSDR_ID_AUDIO_SECONDARY &&
            app->secondaryProbeEnabled) {
          app->secondaryProbeEnabled = false;
          app->secondaryProbeSuppressedCount += 1;
        }
      }
      break;
    }
    case DABSDR_NID_USER_APP_LIST: {
      if (p->len < sizeof(dabsdrNtfUserAppList_t) || p->pData == nullptr) {
        return;
      }
      const auto *ua = static_cast<const dabsdrNtfUserAppList_t *>(p->pData);
      if (ua->getUserAppListItem == nullptr || ua->numUserApps == 0) {
        return;
      }

      uint16_t chosenType = 0;
      for (uint8_t i = 0; i < ua->numUserApps; ++i) {
        dabsdrUserAppListItem_t item{};
        if (ua->getUserAppListItem(app->handle, i, &item) != 0) {
          continue;
        }
        if (item.type != 0) {
          chosenType = item.type;
          break;
        }
      }

      if (chosenType != 0) {
        std::lock_guard<std::mutex> lk(app->mtx);
        app->userAppTypeByComp[makeSidScidsKey(ua->SId, ua->SCIdS)] = chosenType;
      }
      break;
    }
    case DABSDR_NID_PTY: {
      if (p->len < sizeof(dabsdrNtfPTy_t) || p->pData == nullptr) {
        return;
      }
      const auto *pty = static_cast<const dabsdrNtfPTy_t *>(p->pData);
      std::lock_guard<std::mutex> lk(app->mtx);
      auto &svc = app->serviceInfoBySid[pty->SId];
      // Keep previously learned PTY when notification carries unknown 0xFF.
      if (pty->s != 0xFF) {
        svc.ptyS = pty->s;
      }
      if (pty->d != 0xFF) {
        svc.ptyD = pty->d;
      }
      break;
    }
    case DABSDR_NID_TII: {
      if (p->pData == nullptr) {
        return;
      }
      const uint8_t *raw = static_cast<const uint8_t *>(p->pData);

      uint8_t idsRequested = 0;
      if (p->len >= 1) {
        idsRequested = raw[0];
      } else {
        // Some libdabsdr builds report len=0 for fixed notifications.
        const auto *tii = static_cast<const dabsdrNtfTii_t *>(p->pData);
        idsRequested = tii->numIds;
      }
      if (idsRequested > 24) {
        idsRequested = 24;
      }
      if (idsRequested == 0) {
        std::lock_guard<std::mutex> lk(app->mtx);
        app->tiiNotificationCount += 1;
        app->tiiLastLen = p->len;
        app->tiiLastIdsRequested = 0;
        return;
      }

      struct ParsedTii {
        uint8_t main;
        uint8_t sub;
        float level;
      };
      std::vector<ParsedTii> parsed;
      parsed.reserve(idsRequested);

      const size_t alignedOffset = offsetof(dabsdrNtfTii_t, id);
      const size_t alignedStride = sizeof(dabsdrTii_t);
      const size_t packedOffset = 1U;
      const size_t packedStride = 6U;

      const bool alignedNoLenCheck = (p->len == 0);
      bool canUseAligned = false;
      if (!alignedNoLenCheck && p->len >= alignedOffset) {
        const size_t bytesForIds = static_cast<size_t>(p->len) - alignedOffset;
        canUseAligned = ((bytesForIds / alignedStride) >= idsRequested);
      }

      if (alignedNoLenCheck || canUseAligned) {
        const auto *tii = static_cast<const dabsdrNtfTii_t *>(p->pData);
        for (uint8_t i = 0; i < idsRequested; ++i) {
          parsed.push_back(ParsedTii{tii->id[i].main, tii->id[i].sub, tii->id[i].level});
        }
      } else {
        if (p->len <= packedOffset) {
          return;
        }
        size_t available = static_cast<size_t>(p->len) - packedOffset;
        size_t maxPacked = available / packedStride;
        if (maxPacked == 0) {
          return;
        }
        if (maxPacked > idsRequested) {
          maxPacked = idsRequested;
        }

        const uint8_t *ptr = raw + packedOffset;
        for (size_t i = 0; i < maxPacked; ++i) {
          ParsedTii item{};
          item.main = ptr[0];
          item.sub = ptr[1];
          float lvl = 0.0F;
          std::memcpy(&lvl, ptr + 2, sizeof(float));
          item.level = lvl;
          parsed.push_back(item);
          ptr += packedStride;
        }
      }

      if (parsed.empty()) {
        std::lock_guard<std::mutex> lk(app->mtx);
        app->tiiNotificationCount += 1;
        app->tiiLastLen = p->len;
        app->tiiLastIdsRequested = idsRequested;
        return;
      }

      std::lock_guard<std::mutex> lk(app->mtx);
      app->tiiNotificationCount += 1;
      app->tiiNotificationNonZeroCount += 1;
      app->tiiLastLen = p->len;
      app->tiiLastIdsRequested = idsRequested;
      for (const ParsedTii &t : parsed) {
        const int tiiId = static_cast<int>(t.main) * 100 + static_cast<int>(t.sub);
        auto &info = app->tiiById[tiiId];
        info.tii = tiiId;
        info.count += 1;
        if (t.level > info.maxLevel) {
          info.maxLevel = t.level;
        }
      }
      break;
    }
    default:
      break;
  }
}

static void printUsage() {
#if defined(ABRA_INPUT_RTLSDR)
  std::fprintf(stderr,
               "Usage: abra-rtlsdr --channel <11C> [--seconds 18] [--band BAND_III|L_BAND]\n"
               "       [--gain <0..100>] [--autogain] [--ppm <corr>] [--device-index <n>]\n"
               "       [--device-serial <serial>] [--rtl-opts <opts>]\n"
               "       [--audio-details] [--probe-ms <ms>] [--log <file>]\n"
               "\n"
               "dab-raw compatible aliases:\n"
               "  -C <channel>    same as --channel\n"
               "  -B <band>       same as --band\n"
               "  -G <0..100>     same as --gain\n"
               "  -Q              same as --autogain\n"
               "  -p <corr>       same as --ppm\n"
               "  -d <index>      same as --device-index\n"
               "  -s <serial>     same as --device-serial\n"
               "  -O <opts>       same as --rtl-opts\n"
               "  -D              same as --audio-details\n"
               "  -S <hexSid>     filter output to one service id\n"
               "  -c              enable CSV output lines (in addition to ABRA_* lines)\n"
               "  --csv-only      CSV-only mode (suppress ABRA_* lines)\n"
               "  --emit-abra      keep ABRA_* lines in CSV mode (non-dab-raw extension)\n"
               "  --emit-csv-service keep CSV_SERVICE rows (non-dab-raw extension)\n"
               "  --emit-csv-tii   keep CSV_TII rows (non-dab-raw extension)\n"
               "  -E <minSNR>     accepted for compatibility\n"
               "  -n <seconds>    same as --seconds\n");
#else
  std::fprintf(stderr,
               "Usage: abra-raw --file <raw_iq_file> [--seconds 18] [--channel 11C] [--band BAND_III|L_BAND]\n"
               "       [--audio-details] [--rewind] [--rewind-offset <sec>] [--probe-ms <ms>] [--log <file>]\n"
               "       File-input mode: --channel/--band are optional and only used to override tuning metadata.\n"
               "       If --channel is omitted, channel is auto-detected from filename when possible; otherwise baseband decode is used.\n"
               "\n"
               "dab-raw compatible aliases:\n"
               "  -F <file>       same as --file\n"
               "  -C <channel>    same as --channel\n"
               "  -B <band>       same as --band\n"
               "  -D              same as --audio-details\n"
               "  -Y              same as --rewind\n"
               "  -o <seconds>    same as --rewind-offset and file start offset\n"
               "  -X              disable realtime pacing for file playback\n"
               "  -S <hexSid>     filter output to one service id\n"
               "  -c              enable CSV output lines (in addition to ABRA_* lines)\n"
               "  -Q              CSV-only mode (suppress ABRA_* lines)\n"
               "  --emit-abra      keep ABRA_* lines in CSV mode (non-dab-raw extension)\n"
               "  --emit-csv-service keep CSV_SERVICE rows (non-dab-raw extension)\n"
               "  --emit-csv-tii   keep CSV_TII rows (non-dab-raw extension)\n"
               "  -E <minSNR>     accepted for compatibility\n"
               "  -n <seconds>    same as --seconds\n");
#endif
}

static bool parseArgs(AppState &app, int argc, char **argv) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg(argv[i]);
    if ((arg == "--file" || arg == "-F") && i + 1 < argc) {
#if defined(ABRA_INPUT_RTLSDR)
      std::fprintf(stderr, "--file/-F is not supported in abra-rtlsdr mode\n");
      return false;
#else
      app.inputFile = argv[++i];
#endif
    } else if ((arg == "--channel" || arg == "-C") && i + 1 < argc) {
      app.channel = argv[++i];
      app.hasChannel = true;
    } else if ((arg == "--seconds" || arg == "-n") && i + 1 < argc) {
      app.runSeconds = std::atoi(argv[++i]);
      if (app.runSeconds < 1) {
        app.runSeconds = 1;
      }
    } else if ((arg == "--band" || arg == "-B") && i + 1 < argc) {
      const std::string b(argv[++i]);
      app.band = (b == "L_BAND") ? L_BAND : BAND_III;
    } else if (arg == "--audio-details" || arg == "-D") {
      app.detailedAudio = true;
    } else if (arg == "--rewind" || arg == "-Y") {
      app.rewindPerService = true;
    } else if ((arg == "--rewind-offset" || arg == "-o") && i + 1 < argc) {
      app.rewindOffsetSec = std::atof(argv[++i]);
      app.fileOffsetSec = app.rewindOffsetSec;
      if (app.rewindOffsetSec < 0.0) {
        app.rewindOffsetSec = 0.0;
        app.fileOffsetSec = 0.0;
      }
    } else if (arg == "--probe-ms" && i + 1 < argc) {
      app.audioProbeMs = std::atoi(argv[++i]);
      if (app.audioProbeMs < 200) {
        app.audioProbeMs = 200;
      }
    } else if ((arg == "--log" || arg == "-L") && i + 1 < argc) {
      app.logPath = argv[++i];
    } else if (arg == "-X" || arg == "--no-realtime") {
      app.noRealtime = true;
    } else if ((arg == "--gain" || arg == "-G") && i + 1 < argc) {
      app.gain = std::atoi(argv[++i]);
      if (app.gain < 0) {
        app.gain = 0;
      }
      if (app.gain > 100) {
        app.gain = 100;
      }
    } else if (arg == "--autogain") {
      app.autogain = true;
    } else if ((arg == "--ppm" || arg == "-p") && i + 1 < argc) {
      app.ppmCorrection = std::atoi(argv[++i]);
    } else if ((arg == "--device-index" || arg == "-d") && i + 1 < argc) {
      app.deviceIndex = std::atoi(argv[++i]);
      if (app.deviceIndex < 0) {
        app.deviceIndex = 0;
      }
    } else if ((arg == "--device-serial" || arg == "-s") && i + 1 < argc) {
      app.deviceSerial = argv[++i];
    } else if ((arg == "--rtl-opts" || arg == "-O") && i + 1 < argc) {
      app.rtlOpts = argv[++i];
    } else if (arg == "-Q") {
#if defined(ABRA_INPUT_RTLSDR)
      app.autogain = true;
#else
      app.csvOutput = true;
      app.csvOnly = true;
      app.emitAbraLines = false;
#endif
    } else if (arg == "-c" || arg == "--csv") {
      app.csvOutput = true;
      // For drop-in dab-raw compatibility, CSV mode suppresses ABRA_* by default.
      app.emitAbraLines = false;
    } else if (arg == "--csv-only") {
      app.csvOutput = true;
      app.csvOnly = true;
      app.emitAbraLines = false;
    } else if (arg == "--emit-abra") {
      app.emitAbraLines = true;
    } else if (arg == "--emit-csv-service") {
      app.emitCsvService = true;
    } else if (arg == "--emit-csv-tii") {
      app.emitCsvTii = true;
    } else if ((arg == "-S" || arg == "--sid") && i + 1 < argc) {
      if (!parseHexSid(argv[++i], &app.requestedSid)) {
        std::fprintf(stderr, "Invalid SID value for -S/--sid\n");
        return false;
      }
    } else if (arg == "-E" || arg == "--scan") {
      if (i + 1 < argc && argv[i + 1][0] != '-') {
        app.minSnrToExit = std::atoi(argv[++i]);
      } else {
        app.minSnrToExit = 0;
      }
      (void)app.minSnrToExit;
    } else if (arg == "-R" || arg == "-x" || arg == "-v" || arg == "-f") {
      // Accepted for dab-raw CLI compatibility, not used by this tool.
    } else if (arg == "-W" && i + 1 < argc) {
      const int waitMs = std::atoi(argv[++i]);
      if (waitMs > 0) {
        app.runSeconds = std::max(1, (waitMs + 999) / 1000);
      }
    } else if ((arg == "-A" || arg == "-P" || arg == "-p" || arg == "-T" ||
                arg == "-M" || arg == "-t" || arg == "-a" || arg == "-r" || arg == "-z" ||
                arg == "-H" || arg == "-I") &&
               i + 1 < argc) {
      // Accepted for dab-raw CLI compatibility, consume argument and ignore.
      ++i;
    } else if (arg == "--help" || arg == "-h") {
      printUsage();
      return false;
    } else {
      std::fprintf(stderr, "Unknown or incomplete argument: %s\n", arg.c_str());
      printUsage();
      return false;
    }
  }

  if (kUseRtlSdrInput) {
    if (!app.hasChannel) {
      std::fprintf(stderr, "Missing required --channel for abra-rtlsdr\n");
      printUsage();
      return false;
    }
  } else if (app.inputFile.empty()) {
    printUsage();
    return false;
  }
  return true;
}

int main(int argc, char **argv) {
  AppState app;
  if (!parseArgs(app, argc, argv)) {
    return 1;
  }

  if (!app.logPath.empty()) {
    app.logFile = std::fopen(app.logPath.c_str(), "wb");
    if (app.logFile == nullptr) {
      std::fprintf(stderr, "Cannot open log file: %s\n", app.logPath.c_str());
      return 3;
    }
  }

  if (!kUseRtlSdrInput && !app.hasChannel) {
    std::string autoChannel;
    if (detectChannelFromFilename(app.inputFile, &autoChannel)) {
      app.channel = autoChannel;
      app.hasChannel = true;
      printToOutputs(stderr, app.logFile,
                     "Auto-detected channel from filename: %s\n", app.channel.c_str());
    }
  }

  if (app.hasChannel) {
    bandHandler band;
    app.frequency = static_cast<uint32_t>(band.Frequency(app.band, app.channel));
  } else {
    app.frequency = 0;
  }

  dabsdrVersion_t version{0, 0, 0, 0};
  dabsdrGetVersion(&version);
  printToOutputs(stderr, app.logFile, "libdabsdr %u.%u.%u flags=0x%02X\n",
                 static_cast<unsigned>(version.major),
                 static_cast<unsigned>(version.minor),
                 static_cast<unsigned>(version.patch),
                 static_cast<unsigned>(version.flags));

#if defined(ABRA_INPUT_RTLSDR)
  try {
    app.device = new rtlsdrHandler(
        true,
        static_cast<int32_t>(app.frequency),
        static_cast<int16_t>(app.ppmCorrection),
        static_cast<int16_t>(app.gain),
        app.autogain,
        static_cast<uint16_t>(app.deviceIndex),
        app.deviceSerial.empty() ? nullptr : app.deviceSerial.c_str(),
        app.rtlOpts.empty() ? nullptr : app.rtlOpts.c_str());
  } catch (...) {
    printToOutputs(stderr, app.logFile, "Failed to initialize RTL-SDR device\n");
    if (app.logFile != nullptr) {
      std::fclose(app.logFile);
      app.logFile = nullptr;
    }
    return 4;
  }
#else
  if (app.fileOffsetSec > 0.0) {
    app.device = new rawFiles(app.inputFile, app.fileOffsetSec, nullptr, nullptr, app.noRealtime);
  } else {
    app.device = new rawFiles(app.inputFile, true, app.noRealtime);
  }
  rawFiles *rawDev = dynamic_cast<rawFiles *>(app.device);
  if (rawDev != nullptr) {
    rawDev->setVerbose(false);
  }
#endif

  app.device->restartReader(static_cast<int32_t>(app.frequency));

  if (dabsdrInit(&app.handle) != 0 || app.handle == nullptr) {
    printToOutputs(stderr, app.logFile, "dabsdrInit failed\n");
    app.device->stopReader();
    delete app.device;
    if (app.logFile != nullptr) {
      std::fclose(app.logFile);
    }
    return 2;
  }

  gApp = &app;
  dabsdrRegisterInputFcn(app.handle, inputCallback);
  dabsdrRegisterDummyInputFcn(app.handle, dummyInputCallback);
  dabsdrRegisterNotificationCb(app.handle, notificationCallback, &app);
  dabsdrRegisterAudioCb(app.handle, audioCallback, &app);

  if (app.hasChannel) {
    dabsdrRequest_Tune(app.handle, app.frequency);
  }
  dabsdrRequest_GetEnsemble(app.handle);
  // Start with default TII mode and then re-arm with conservative mode
  // on retries; some muxes/devices react differently across modes.
  dabsdrRequest_SetTII(app.handle, 1, DABSDR_TII_MODE_DEFAULT);
  dabsdrRequest_SetPeriodicNotify(app.handle, 1, 0xFFFFFFFFu);

  // libdabsdr starts its own internal processing thread.
  dabsdr(app.handle);

  const auto tEnd = std::chrono::steady_clock::now() + std::chrono::seconds(app.runSeconds);
  auto nextDiscoveryRetry = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
  while (std::chrono::steady_clock::now() < tEnd) {
    const auto now = std::chrono::steady_clock::now();
    if (now >= nextDiscoveryRetry && app.gotSync.load()) {
      bool noServices = false;
      {
        std::lock_guard<std::mutex> lk(app.mtx);
        noServices = app.services.empty();
      }

      if (noServices) {
        dabsdrRequest_GetEnsemble(app.handle);
        dabsdrRequest_GetServiceList(app.handle);
      }
      nextDiscoveryRetry = now + std::chrono::milliseconds(300);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  if (app.gotSync.load()) {
    bool haveTii = false;
    {
      std::lock_guard<std::mutex> lk(app.mtx);
      haveTii = !app.tiiById.empty();
    }

    if (!haveTii) {
      // One conservative re-arm and short dwell; avoid frequent re-arms that
      // can keep detector state from settling.
      dabsdrRequest_SetTII(app.handle, 1, DABSDR_TII_MODE_CONSERVATIVE);
      std::this_thread::sleep_for(std::chrono::milliseconds(1800));
    }
  }

  if (app.gotServiceList.load()) {
    // Give async component discovery a short settle window so CSV_AUDIO/
    // CSV_PACKET are available in short scans.
    const auto compWaitEnd = std::chrono::steady_clock::now() + std::chrono::milliseconds(2200);
    while (std::chrono::steady_clock::now() < compWaitEnd) {
      std::vector<uint32_t> missing;
      {
        std::lock_guard<std::mutex> lk(app.mtx);
        for (const auto &kv : app.services) {
          const uint32_t sid = kv.first;
          const bool haveAudioEntry = (app.audioComponents.find(sid) != app.audioComponents.end());
          const bool havePacketEntry = (app.packetComponents.find(sid) != app.packetComponents.end());
          if (!haveAudioEntry && !havePacketEntry) {
            missing.push_back(sid);
          }
        }
      }

      if (missing.empty()) {
        break;
      }

      for (const uint32_t sid : missing) {
        dabsdrRequest_GetServiceComponents(app.handle, sid);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(90));
    }
  }

  probeAudioDetails(app);

  dabsdrRequest_Exit(app.handle);

  std::map<uint32_t, std::string> services;
  std::map<uint32_t, std::vector<AudioComponentInfo>> audioComponents;
  std::map<uint32_t, ServiceInfo> serviceInfoBySid;
  std::map<uint32_t, std::vector<PacketComponentInfo>> packetComponents;
  std::unordered_map<uint64_t, uint16_t> userAppTypeByComp;
  std::unordered_map<uint64_t, AudioProbeInfo> audioProbe;
  std::unordered_map<uint64_t, bool> audioProbeSelectionOk;
  std::map<int, TiiInfo> tiiById;
  std::string ensembleLabel;
  std::string ensembleShortLabel;
  uint16_t eid = 0;
  uint8_t ecc = 0;
  uint8_t intTable = 0;
  int snrMin = 0;
  int snrMax = 0;
  int snrCount = 0;
  int snrAvg = 0;
  int ficMin = 0;
  int ficMax = 0;
  int ficCount = 0;
  int ficAvg = 0;
  uint32_t audioCallbackCount = 0;
  uint32_t audioCallbackCountDuringProbe = 0;
  uint32_t audioCallbackPrimaryCount = 0;
  uint32_t audioCallbackSecondaryCount = 0;
  uint32_t tiiNotificationCount = 0;
  uint32_t tiiNotificationNonZeroCount = 0;
  uint16_t tiiLastLen = 0;
  uint8_t tiiLastIdsRequested = 0;
  uint32_t serviceSelNotificationCount = 0;
  uint32_t serviceSelNotificationLenZeroCount = 0;
  uint32_t serviceSelSuccessCount = 0;
  uint32_t serviceSelNotReadyCount = 0;
  uint32_t serviceSelOtherStatusCount = 0;
  uint32_t secondaryProbeSuppressedCount = 0;
  uint32_t inputCallbackCalls = 0;
  uint32_t inputCallbackZeroReads = 0;
  uint32_t inputSamplesRequested = 0;
  uint32_t inputSamplesDelivered = 0;
  uint32_t dummyCallbackCalls = 0;
  {
    std::lock_guard<std::mutex> lk(app.mtx);
    services = app.services;
    serviceInfoBySid = app.serviceInfoBySid;
    audioComponents = app.audioComponents;
    packetComponents = app.packetComponents;
    userAppTypeByComp = app.userAppTypeByComp;
    audioProbe = app.audioProbe;
    audioProbeSelectionOk = app.audioProbeSelectionOk;
    tiiById = app.tiiById;
    ensembleLabel = app.ensembleLabel;
    ensembleShortLabel = app.ensembleShortLabel;
    eid = app.ensembleEID;
    ecc = app.ensembleECC;
    intTable = app.ensembleIntTable;

    snrCount = app.snrCount;
    ficCount = app.ficCount;
    if (app.snrCount > 0) {
      snrMin = app.snrMin;
      snrMax = app.snrMax;
      snrAvg = app.snrSum / app.snrCount;
    }
    if (app.ficCount > 0) {
      ficMin = app.ficMin;
      ficMax = app.ficMax;
      ficAvg = app.ficSum / app.ficCount;
    } else if (app.snrCount > 0) {
      // Keep dab-raw-like defaults when periodic FIC stats are unavailable.
      ficMin = 100;
      ficMax = 100;
      ficAvg = 100;
      ficCount = app.snrCount;
    }

    if (ecc == 0U) {
      ecc = inferEnsembleEccFromServices(services);
    }
    audioCallbackCount = app.audioCallbackCount;
    audioCallbackCountDuringProbe = app.audioCallbackCountDuringProbe;
    audioCallbackPrimaryCount = app.audioCallbackPrimaryCount;
    audioCallbackSecondaryCount = app.audioCallbackSecondaryCount;
    tiiNotificationCount = app.tiiNotificationCount;
    tiiNotificationNonZeroCount = app.tiiNotificationNonZeroCount;
    tiiLastLen = app.tiiLastLen;
    tiiLastIdsRequested = app.tiiLastIdsRequested;
    serviceSelNotificationCount = app.serviceSelNotificationCount;
    serviceSelNotificationLenZeroCount = app.serviceSelNotificationLenZeroCount;
    serviceSelSuccessCount = app.serviceSelSuccessCount;
    serviceSelNotReadyCount = app.serviceSelNotReadyCount;
    serviceSelOtherStatusCount = app.serviceSelOtherStatusCount;
    secondaryProbeSuppressedCount = app.secondaryProbeSuppressedCount;
    inputCallbackCalls = app.inputCallbackCalls;
    inputCallbackZeroReads = app.inputCallbackZeroReads;
    inputSamplesRequested = app.inputSamplesRequested;
    inputSamplesDelivered = app.inputSamplesDelivered;
    dummyCallbackCalls = app.dummyCallbackCalls;
  }

  const char *channelStr = app.hasChannel ? app.channel.c_str() : "(none)";
  const bool csvScanNoSpecificSid = app.csvOutput && app.requestedSid == -1;
  const bool hasValidEnsemble =
      (eid != 0) && !ensembleLabel.empty() && (ensembleLabel != "unknown ensemble");
  const bool noDecoderAudioCallbacks = (audioCallbackCount == 0);
  const bool csvGenericCodecMode = csvScanNoSpecificSid && !app.detailedAudio;
  if (app.emitAbraLines && !app.csvOnly) {
    printToOutputs(stdout, app.logFile,
                   "ABRA_DEBUG callbacks audio_total=%u audio_probe=%u audio_pri=%u audio_sec=%u sel_ntf=%u sel_len0=%u sel_ok=%u sel_not_ready=%u sel_other=%u sel_sec_supp=%u tii_ntf=%u tii_nonzero=%u tii_last_len=%u tii_last_ids=%u input_calls=%u input_zero=%u input_req=%u input_got=%u dummy_calls=%u\n",
                   static_cast<unsigned>(audioCallbackCount),
                   static_cast<unsigned>(audioCallbackCountDuringProbe),
                   static_cast<unsigned>(audioCallbackPrimaryCount),
                   static_cast<unsigned>(audioCallbackSecondaryCount),
                   static_cast<unsigned>(serviceSelNotificationCount),
                   static_cast<unsigned>(serviceSelNotificationLenZeroCount),
                   static_cast<unsigned>(serviceSelSuccessCount),
                   static_cast<unsigned>(serviceSelNotReadyCount),
                   static_cast<unsigned>(serviceSelOtherStatusCount),
                   static_cast<unsigned>(secondaryProbeSuppressedCount),
                   static_cast<unsigned>(tiiNotificationCount),
                   static_cast<unsigned>(tiiNotificationNonZeroCount),
                   static_cast<unsigned>(tiiLastLen),
                   static_cast<unsigned>(tiiLastIdsRequested),
                   static_cast<unsigned>(inputCallbackCalls),
                   static_cast<unsigned>(inputCallbackZeroReads),
                   static_cast<unsigned>(inputSamplesRequested),
                   static_cast<unsigned>(inputSamplesDelivered),
                   static_cast<unsigned>(dummyCallbackCalls));

    if (app.detailedAudio && noDecoderAudioCallbacks) {
      printToOutputs(stderr, app.logFile,
                     "warning: detailed audio unavailable: libdabsdr emitted no audio callbacks in this run (cannot derive HE-AAC profile/rate)\n");
    }

    if (hasValidEnsemble || !app.csvOutput) {
      printToOutputs(stdout, app.logFile,
                     "ABRA_SCAN_RESULT channel=%s freq=%u sync=%d serviceList=%d services=%u ensemble=\"%s\" eid=%04X ecc=%02X\n",
                     channelStr, app.frequency,
                     app.gotSync.load() ? 1 : 0,
                     app.gotServiceList.load() ? 1 : 0,
                     static_cast<unsigned>(services.size()),
                     ensembleLabel.c_str(),
                     static_cast<unsigned>(eid),
                     static_cast<unsigned>(ecc));
    }
  }

  if (!hasValidEnsemble) {
    dabsdrDeinit(&app.handle);
    app.device->stopReader();
    delete app.device;
    gApp = nullptr;

    if (app.logFile != nullptr) {
      std::fclose(app.logFile);
      app.logFile = nullptr;
    }
    return 0;
  }

  for (const auto &kv : services) {
    if (app.requestedSid != -1 && static_cast<int>(kv.first) != app.requestedSid) {
      continue;
    }
    if (app.emitAbraLines && !app.csvOnly) {
      printToOutputs(stdout, app.logFile, "ABRA_SERVICE sid=%08X label=\"%s\"\n",
                     static_cast<unsigned>(kv.first), kv.second.c_str());
    }
  }

  if (app.csvOutput) {
    const long secsEpoch = static_cast<long>(std::time(nullptr));
    const TiiStrengthContext tiiStrengthCtx = buildTiiStrengthContext(tiiById);

    std::vector<TiiInfo> tiiVec;
    tiiVec.reserve(tiiById.size());
    for (const auto &kv : tiiById) {
      if (kv.second.tii > 0 && kv.second.count > 0) {
        tiiVec.push_back(kv.second);
      }
    }
    std::sort(tiiVec.begin(), tiiVec.end(), [](const TiiInfo &a, const TiiInfo &b) {
      if (a.count != b.count) {
        return a.count > b.count;
      }
      return a.tii < b.tii;
    });

    const size_t tiiGroups = tiiVec.empty() ? 1U : tiiVec.size();
    std::string csvEnsHeader = std::to_string(secsEpoch);
    csvEnsHeader += ",#CSV_ENSEMBLE,channel,ensembleName,snr,min_snr,max_snr,num_snr,avg_snr,fic,min_fic,max_fic,num_fic,avg_fic";
    csvEnsHeader += ",tii";
    for (size_t i = 0; i < tiiGroups; ++i) {
      csvEnsHeader += ",tii_id,num,max (avg_snr),max (min_snr),max (next_snr),";
    }
    csvEnsHeader += ",,shortLabel,label";
    printToOutputs(stdout, app.logFile, "%s\n", csvEnsHeader.c_str());

    std::map<int16_t, int> protHistogram;
    for (const auto &svc : audioComponents) {
      for (const auto &comp : svc.second) {
        bool sf = false;
        int16_t pl = 0;
        decodeProtection(comp.protectionLevel, comp.uepIdx, comp.bitrate, comp.subchSize, &sf, &pl);
        if (!sf) {
          protHistogram[pl] += 1;
        }
      }
    }
    int16_t dominantAudioProtLevel = -1;
    int dominantAudioProtCount = 0;
    for (const auto &kv : protHistogram) {
      if (kv.second > dominantAudioProtCount) {
        dominantAudioProtCount = kv.second;
        dominantAudioProtLevel = kv.first;
      }
    }

    std::string csvEnsLine = std::to_string(secsEpoch);
    csvEnsLine += ",CSV_ENSEMBLE," + csvQuote(channelStr);
    {
      char eidBuf[32]{};
      std::snprintf(eidBuf, sizeof(eidBuf), "0x%04x", static_cast<unsigned>(eid));
      csvEnsLine += "," + std::string(eidBuf);
    }
    csvEnsLine += "," + csvQuote(ensembleLabel);
    csvEnsLine += ",\"snr\"," + std::to_string(snrMin) + "," + std::to_string(snrMax) + "," +
                  std::to_string(snrCount) + "," + std::to_string(snrAvg);
    csvEnsLine += ",\"fic\"," + std::to_string(ficMin) + "," + std::to_string(ficMax) + "," +
                  std::to_string(ficCount) + "," + std::to_string(ficAvg);

    csvEnsLine += ",\"tii\"";
    if (tiiVec.empty()) {
      csvEnsLine += ",,,,,,";
    } else {
      for (const auto &t : tiiVec) {
        // Map dabsdr TII level to a relative strength: strongest == 100.
        const float relStrengthPct = computeTiiStrengthPct(t, tiiStrengthCtx);
        char relBuf[32]{};
        std::snprintf(relBuf, sizeof(relBuf), "%.1f", static_cast<double>(relStrengthPct));
        // max(avg_snr) carries relative strength in percent; other extended fields remain unavailable.
        csvEnsLine += "," + std::to_string(t.tii) + "," + std::to_string(t.count) + "," +
                      std::string(relBuf) + ",,,";
      }
    }

    csvEnsLine += ",," + csvQuote("shortLabel") + "," + csvQuote(ensembleShortLabel);
    printToOutputs(stdout, app.logFile, "%s\n", csvEnsLine.c_str());

    if (app.emitCsvService) {
      for (const auto &kv : services) {
        if (app.requestedSid != -1 && static_cast<int>(kv.first) != app.requestedSid) {
          continue;
        }
        printToOutputs(stdout, app.logFile,
                       "CSV_SERVICE,%08X,\"%s\",\"%s\",%u,%s,%s\n",
                       static_cast<unsigned>(kv.first), kv.second.c_str(), channelStr,
                       static_cast<unsigned>(app.frequency),
                       toHex(static_cast<uint32_t>(eid)).c_str(),
                       toHex(static_cast<uint32_t>(ecc)).c_str());
      }
    }

    for (const auto &svc : audioComponents) {
      if (app.requestedSid != -1 && static_cast<int>(svc.first) != app.requestedSid) {
        continue;
      }
      const ServiceInfo *si = nullptr;
      const auto sit = serviceInfoBySid.find(svc.first);
      if (sit != serviceInfoBySid.end()) {
        si = &sit->second;
      }
      const SidView sidView = resolveSidView(svc.first, ecc);
      const uint8_t pty = effectivePty(si);
      const uint8_t ptyOut = (pty == 0xFF) ? static_cast<uint8_t>(0) : pty;
      const uint8_t ctry = sidView.countryId;
      const uint8_t effectiveEcc = sidView.effectiveEcc;
      const char *countryName = getCountry(effectiveEcc, ctry);
      const std::string serviceLabel = lookupServiceLabel(services, svc.first);
      for (const auto &comp : svc.second) {
        const uint64_t key = makeSidScidsKey(svc.first, comp.scids);
        const auto it = audioProbe.find(key);
        const AudioProbeInfo *probe = (it != audioProbe.end()) ? &it->second : nullptr;
        const auto selIt = audioProbeSelectionOk.find(key);
        const bool selectionOk = (selIt != audioProbeSelectionOk.end() && selIt->second);
        const std::string codec = (csvGenericCodecMode || noDecoderAudioCallbacks)
                                      ? codecDescriptionCsvScanFallback(comp.ascty)
                    : codecDescriptionWithRate(comp.ascty, probe, selectionOk);
        const uint32_t frames = (probe != nullptr) ? probe->frames : 0;
        // Debug: log frame count for services without full codec detection
        if (frames < kMinReliableAudioFrames) {
          fprintf(stderr, "AUDIO_FRAMES: SId=%04X frames=%u selectionOk=%d codec=\"%s\"\n",
                  static_cast<unsigned>(svc.first), frames, selectionOk ? 1 : 0, codec.c_str());
        }
        bool shortForm = false;
        int16_t protLevel = 0;
        decodeProtection(comp.protectionLevel, comp.uepIdx, comp.bitrate, comp.subchSize,
             &shortForm, &protLevel);
        const char *protTxt = getProtectionLevel(shortForm, protLevel);
        const char *codeRateTxt = getCodeRate(shortForm, protLevel);
        const bool gotInterTable = (intTable != 0);
        const std::string shortLabel = (si != nullptr && !si->shortLabel.empty()) ? si->shortLabel : "";
        const bool langUnknown = (comp.lang == 0);
        std::string outLine = std::to_string(secsEpoch);
        outLine += ",CSV_AUDIO," + csvQuote(channelStr);
        outLine += "," + toHex(static_cast<uint32_t>(eid));
        outLine += "," + csvQuote(ensembleLabel);
        outLine += "," + toHex(static_cast<uint32_t>(sidView.displaySid));
        outLine += "," + csvQuote(serviceLabel);
        outLine += "," + std::to_string(static_cast<unsigned>(comp.scids));
        outLine += "," + std::to_string(static_cast<unsigned>(ctry));
        outLine += "," + csvQuote(protTxt);
        outLine += "," + std::to_string(static_cast<unsigned>(protLevel));
        outLine += "," + csvQuote(codeRateTxt);
        outLine += "," + std::to_string(static_cast<unsigned>(comp.bitrate));
        outLine += "," + std::to_string(static_cast<unsigned>(comp.ascty));
        outLine += "," + csvQuote(codec);
        outLine += "," + toHex(static_cast<uint32_t>(effectiveEcc));
        outLine += "," + toHex(static_cast<uint32_t>(ctry));
        outLine += "," + csvQuote(countryName != nullptr ? countryName : "unknown");
        outLine += "," + std::string(langUnknown ? "-1" : std::to_string(static_cast<int>(comp.lang)));
        outLine += "," + csvQuote(langUnknown ? "unknown (Lang.)" : getLanguage(comp.lang));
        outLine += "," + std::to_string(static_cast<unsigned>(ptyOut));
        outLine += "," + csvQuote(getProgramType(gotInterTable, intTable, ptyOut));
        outLine += "," + std::to_string(static_cast<unsigned>(comp.subchSize));
        outLine += "," + std::to_string(static_cast<unsigned>(comp.subchId));
        outLine += "," + csvQuote(shortLabel);
        outLine += "," + csvQuote(comp.label);
        outLine += "," + csvQuote("");
        printToOutputs(stdout, app.logFile, "%s\n", outLine.c_str());
      }
    }

    for (const auto &svc : packetComponents) {
      if (app.requestedSid != -1 && static_cast<int>(svc.first) != app.requestedSid) {
        continue;
      }
      const SidView sidView = resolveSidView(svc.first, ecc);
      const uint8_t ctry = sidView.countryId;
      const uint8_t effectiveEcc = sidView.effectiveEcc;
      const char *countryName = getCountry(effectiveEcc, ctry);
      const std::string serviceLabel = lookupServiceLabel(services, svc.first);
      for (const auto &comp : svc.second) {
        bool shortForm = false;
        int16_t protLevel = 0;
        decodeProtection(comp.protectionLevel, comp.uepIdx, 0, comp.subchSize,
             &shortForm, &protLevel);

        // Only fall back to dominant audio protection when libdabsdr gave no
        // explicit protection level (raw == 0). For raw values 6..13 the new
        // decodeProtection maps reliably; overriding with audio level is wrong.
        const bool packetProtectionAmbiguous =
            (!shortForm) && (dominantAudioProtLevel >= 0) &&
            (comp.protectionLevel == 0);
        if (packetProtectionAmbiguous) {
          protLevel = dominantAudioProtLevel;
          shortForm = false;
        }

           const uint16_t packetBitrate = estimateBitrateFromProtection(shortForm, protLevel, comp.subchSize);
        uint16_t userAppType = 0;
        const auto uait = userAppTypeByComp.find(makeSidScidsKey(svc.first, comp.scids));
        if (uait != userAppTypeByComp.end()) {
          userAppType = uait->second;
        }
        std::string outLine = std::to_string(secsEpoch);
        outLine += ",CSV_PACKET," + csvQuote(channelStr);
        outLine += "," + toHex(static_cast<uint32_t>(eid));
        outLine += "," + csvQuote(ensembleLabel);
        outLine += "," + toHex(static_cast<uint32_t>(sidView.displaySid));
        outLine += "," + csvQuote(serviceLabel);
        outLine += "," + std::to_string(static_cast<unsigned>(comp.scids));
        outLine += "," + std::to_string(static_cast<unsigned>(protLevel));
        outLine += "," + csvQuote(getProtectionLevel(shortForm, protLevel));
        outLine += "," + csvQuote(getCodeRate(shortForm, protLevel));
        outLine += "," + std::to_string(static_cast<unsigned>(comp.dscty));
        outLine += "," + csvQuote(getDSCTy(comp.dscty));
        outLine += "," + std::to_string(static_cast<unsigned>(packetBitrate));
        outLine += "," + std::to_string(static_cast<int>(comp.uepIdx));
        outLine += "," + csvQuote(getFECscheme(static_cast<int16_t>(comp.uepIdx)));
        outLine += "," + std::to_string(static_cast<unsigned>(comp.dgFlag));
        outLine += "," + toHex(static_cast<uint32_t>(effectiveEcc));
        outLine += "," + toHex(static_cast<uint32_t>(ctry));
        outLine += "," + csvQuote(countryName != nullptr ? countryName : "unknown");
        outLine += "," + std::to_string(static_cast<unsigned>(userAppType));
        outLine += "," + csvQuote(normalizeUserAppText(userAppType));
        outLine += "," + std::to_string(static_cast<unsigned>(comp.subchSize));
        outLine += "," + std::to_string(static_cast<unsigned>(comp.subchId));
        const auto sit = serviceInfoBySid.find(svc.first);
        const std::string svcShort = (sit != serviceInfoBySid.end()) ? sit->second.shortLabel : "";
        outLine += "," + csvQuote(svcShort);
        outLine += "," + csvQuote("");
        outLine += "," + csvQuote("");
        printToOutputs(stdout, app.logFile, "%s\n", outLine.c_str());
      }
    }
  }

  if (app.detailedAudio && app.emitAbraLines && !app.csvOnly) {
    for (const auto &svc : audioComponents) {
      if (app.requestedSid != -1 && static_cast<int>(svc.first) != app.requestedSid) {
        continue;
      }
      for (const auto &comp : svc.second) {
        const uint64_t key = makeSidScidsKey(svc.first, comp.scids);
        const auto it = audioProbe.find(key);
        const AudioProbeInfo *probe = (it != audioProbe.end()) ? &it->second : nullptr;
        const auto selIt = audioProbeSelectionOk.find(key);
        const bool selectionOk = (selIt != audioProbeSelectionOk.end() && selIt->second);
        const std::string codec = (csvGenericCodecMode || noDecoderAudioCallbacks)
                                      ? codecDescriptionCsvScanFallback(comp.ascty)
                    : codecDescriptionWithRate(comp.ascty, probe, selectionOk);
        bool shortForm = false;
        int16_t protLevel = 0;
        decodeProtection(comp.protectionLevel, comp.uepIdx, comp.bitrate, comp.subchSize,
             &shortForm, &protLevel);
        const char *protTxt = getProtectionLevel(shortForm, protLevel);
        const char *codeRateTxt = getCodeRate(shortForm, protLevel);
        const char *chMode = channelModeText(probe, comp.ascty);
        const uint32_t frames = (probe != nullptr) ? probe->frames : 0;
        const int sbr = (probe != nullptr) ? probe->sbrFlag : -1;
        const int ps = (probe != nullptr) ? probe->psFlag : -1;
        const int aacMode = (probe != nullptr) ? probe->aacChannelMode : -1;

        printToOutputs(stdout, app.logFile,
             "ABRA_AUDIO sid=%08X scids=%u label=\"%s\" ascty=%u bitrate=%u subchSize=%u uepIdx=%u protectionRaw=%u protection=%d protectionText=\"%s\" codeRate=\"%s\" frames=%u codec=\"%s\" channelMode=%s sbr=%d ps=%d aacMode=%d\n",
                       static_cast<unsigned>(svc.first),
                       static_cast<unsigned>(comp.scids),
                       comp.label.c_str(),
                       static_cast<unsigned>(comp.ascty),
                       static_cast<unsigned>(comp.bitrate),
                 static_cast<unsigned>(comp.subchSize),
                 static_cast<unsigned>(comp.uepIdx),
                       static_cast<unsigned>(comp.protectionLevel),
               static_cast<int>(protLevel),
               protTxt,
               codeRateTxt,
                       static_cast<unsigned>(frames),
                       codec.c_str(),
                 chMode,
                       sbr,
                       ps,
                       aacMode);
      }
    }
  }

  if (app.emitAbraLines && !app.csvOnly) {
    const TiiStrengthContext tiiStrengthCtx = buildTiiStrengthContext(tiiById);
    const std::string tiiSummary = std::string("ABRA_TII_SUMMARY count=") +
                                   std::to_string(static_cast<unsigned>(tiiById.size()));
    printToOutputs(stdout, app.logFile, "%s\n", tiiSummary.c_str());
    for (const auto &kv : tiiById) {
      const TiiInfo &t = kv.second;
      const float relStrengthPct = computeTiiStrengthPct(t, tiiStrengthCtx);
      printToOutputs(stdout, app.logFile,
                     "ABRA_TII id=%04d occurrences=%d maxLevel=%.3f relStrengthPct=%.1f\n",
                     t.tii, t.count, t.maxLevel, relStrengthPct);
    }
  }

  if (app.csvOutput && app.emitCsvTii) {
    const TiiStrengthContext tiiStrengthCtx = buildTiiStrengthContext(tiiById);
    for (const auto &kv : tiiById) {
      const TiiInfo &t = kv.second;
      const float relStrengthPct = computeTiiStrengthPct(t, tiiStrengthCtx);
      printToOutputs(stdout, app.logFile,
                     "CSV_TII,%04d,%d,%.3f,%.1f\n",
                     t.tii, t.count, t.maxLevel, relStrengthPct);
    }
  }

  dabsdrDeinit(&app.handle);
  app.device->stopReader();
  delete app.device;
  gApp = nullptr;

  if (app.logFile != nullptr) {
    std::fclose(app.logFile);
    app.logFile = nullptr;
  }

  return 0;
}
