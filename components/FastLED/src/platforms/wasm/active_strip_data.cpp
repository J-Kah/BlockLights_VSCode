
#ifdef __EMSCRIPTEN__

#include <memory>

#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>
#include <emscripten/val.h>

#include "fixed_map.h"
#include "singleton.h"
#include "slice.h"

#include "active_strip_data.h"
#include "engine_events.h"
#include "fixed_map.h"
#include "js.h"
#include "str.h"
#include "namespace.h"
#include <stdio.h>

FASTLED_NAMESPACE_BEGIN

ActiveStripData& ActiveStripData::Instance() {
    return Singleton<ActiveStripData>::instance();
}

void ActiveStripData::update(int id, uint32_t now, const uint8_t* pixel_data, size_t size) {
    mStripMap.update(id, SliceUint8(pixel_data, size));
}

void ActiveStripData::updateScreenMap(int id, const ScreenMap& screenmap) {
    mScreenMap.update(id, screenmap);
}

emscripten::val ActiveStripData::getPixelData_Uint8(int stripIndex) {
    // Efficient, zero copy conversion from internal data to JavaScript.
    SliceUint8 stripData;
    if (mStripMap.get(stripIndex, &stripData)) {
        const uint8_t *data = stripData.data();
        uint8_t *data_mutable = const_cast<uint8_t *>(data);
        size_t size = stripData.size();
        return emscripten::val(
            emscripten::typed_memory_view(size, data_mutable));
    }
    return emscripten::val::undefined();
}

Str ActiveStripData::infoJsonString() {
    ArduinoJson::JsonDocument doc;
    auto array = doc.to<ArduinoJson::JsonArray>();

    for (const auto &[stripIndex, stripData] : mStripMap) {
        auto obj = array.add<ArduinoJson::JsonObject>();
        obj["strip_id"] = stripIndex;
        obj["type"] = "r8g8b8";
    }

    Str jsonBuffer;
    serializeJson(doc, jsonBuffer);
    return jsonBuffer;
}

static ActiveStripData* getActiveStripDataPtr() {
    ActiveStripData* instance = &Singleton<ActiveStripData>::instance();
    return instance;
}

EMSCRIPTEN_BINDINGS(engine_events_constructors) {
    emscripten::class_<ActiveStripData>("ActiveStripData")
        .constructor(&getActiveStripDataPtr, emscripten::allow_raw_pointers())
        .function("getPixelData_Uint8", &ActiveStripData::getPixelData_Uint8);
}


// gcc constructor to get the 
// ActiveStripData instance created.
__attribute__((constructor))
void __init_ActiveStripData() {
    ActiveStripData::Instance();
}

#endif