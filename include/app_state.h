#pragma once

// ============================================================
//  app_state.h  –  App-level mode enum & sub-screen enum
// ============================================================

enum AppMode {
    MODE_BUDDY      = 0,
    MODE_DATETIME   = 1,
    MODE_WEATHER_P  = 2,   // primary city weather
    MODE_WEATHER_S  = 3,   // secondary city weather
    MODE_ALARM      = 4,
    MODE_COUNT      = 5
};

enum AlarmSubScreen {
    ALARM_SUB_ALARM      = 0,
    ALARM_SUB_COUNTDOWN  = 1,
    ALARM_SUB_STOPWATCH  = 2,
    ALARM_SUB_COUNT      = 3
};

// Buddy mood cycling (see spec)
enum BuddyMood {
    MOOD_DEFAULT = 0,
    MOOD_HAPPY,
    MOOD_TIRED,
    MOOD_ANGRY,
    MOOD_COUNT
};

// Long-press sequence: D H D H D H D T A → repeats
// Index:               0 1 2 3 4 5 6 7 8
static const BuddyMood MOOD_SEQUENCE[] = {
    MOOD_DEFAULT, MOOD_HAPPY,
    MOOD_DEFAULT, MOOD_HAPPY,
    MOOD_DEFAULT, MOOD_HAPPY,
    MOOD_DEFAULT,             // 7th long press → Tired
    MOOD_TIRED,               // 8th → Angry
    MOOD_ANGRY
};
static const int MOOD_SEQUENCE_LEN = 9;

struct AppState {
    AppMode       mode          = MODE_BUDDY;
    AlarmSubScreen alarmSub     = ALARM_SUB_ALARM;
    BuddyMood     buddyMood     = MOOD_DEFAULT;
    int           moodSeqIdx    = 0;

    // Whether long-press in datetime/weather is showing secondary info
    bool          showSecondary = false;
};

extern AppState appState;
