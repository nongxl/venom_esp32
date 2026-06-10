#ifndef IORGANISM_H
#define IORGANISM_H

#include <M5Unified.h>
#include <ArduinoJson.h>

struct AudioFeatures; // Forward declaration
class Container;      // Forward declaration

class IOrganism {
public:
    virtual ~IOrganism() = default;
    
    // Core lifecycle
    virtual void update(float gx, float gy, float gz, float soundLevel = 0, float lux = 0) = 0;
    virtual void draw(M5Canvas* canvas, Container* container, float ax, float ay, float az) = 0;
    
    // Interaction
    virtual void setStartled() = 0;
    virtual void toggleDebug() = 0;
    virtual bool isDebugVisible() const = 0;
    virtual void drawDebug(M5Canvas* canvas) = 0;
    
    // Cloud / AI Integration
    virtual String getPhysiologyJson() = 0;
    virtual String getPerceptions() = 0;
    virtual void updateStateFromAudio(const AudioFeatures& features) = 0;
    virtual void updateStateFromLLM(const String& jsonEmotion) = 0;
    virtual void addRecentEvent(const String& event) = 0;
    virtual String getRecentEventsString() = 0;
    
    // Sync mechanisms
    virtual bool pullAIPendingSync() = 0;
    virtual void triggerAISync() = 0;
    virtual void setAISyncInterval(uint32_t ms) = 0;
    virtual uint32_t getAISyncInterval() const = 0;
    
    // Cloud / AI State Transitions
    virtual void notifyAISyncStarted() {}
    virtual void notifyAIThinkingStarted() {}
    virtual void notifyAIResponseReceived() {}
};

#endif // IORGANISM_H
