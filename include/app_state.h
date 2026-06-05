#pragma once

// ============================================================
//  app_state.h  -  Uygulama modu ve durum yonetimi
// ============================================================

enum AppMode {
    MODE_BUDDY      = 0,
    MODE_DATETIME   = 1,
    MODE_WEATHER_P  = 2,   // ana sehir hava durumu
    MODE_WEATHER_S  = 3,   // ikincil sehir hava durumu
    MODE_ALARM      = 4,
    MODE_COUNT      = 5
};

enum AlarmSubScreen {
    ALARM_SUB_ALARM      = 0,
    ALARM_SUB_COUNTDOWN  = 1,
    ALARM_SUB_STOPWATCH  = 2,
    ALARM_SUB_COUNT      = 3
};

// Buddy mood siralamasi
enum BuddyMood {
    MOOD_DEFAULT = 0,
    MOOD_HAPPY,
    MOOD_TIRED,
    MOOD_ANGRY,
    MOOD_COUNT
};

// Uzun basim sirasi: D H D H D H D T A -> dongu
// Index:              0 1 2 3 4 5 6 7 8
static const BuddyMood MOOD_SEQUENCE[] = {
    MOOD_DEFAULT, MOOD_HAPPY,
    MOOD_DEFAULT, MOOD_HAPPY,
    MOOD_DEFAULT, MOOD_HAPPY,
    MOOD_DEFAULT,
    MOOD_TIRED,
    MOOD_ANGRY
};
static const int MOOD_SEQUENCE_LEN = 9;

struct AppState {
    AppMode        mode          = MODE_BUDDY;
    AlarmSubScreen alarmSub      = ALARM_SUB_ALARM;
    BuddyMood      buddyMood     = MOOD_DEFAULT;
    int            moodSeqIdx    = 0;
    bool           showSecondary = false;

    // Alarm modunda hangi alarm yuvasi secili (0-2)
    uint8_t        selectedAlarm = 0;
};

// ============================================================
//  Mod gecis yardimcisi - ikincil konum tanimli degilse
//  MODE_WEATHER_S ve MODE_DATETIME ikincil ekranini atlar.
//  hasSecondary: storage.hasSecondaryCity() degeri
// ============================================================
inline AppMode nextModeSkipSecondary(AppMode current, bool hasSecondary) {
    int next = ((int)current + 1) % MODE_COUNT;

    // Ikincil konum yoksa MODE_WEATHER_S'i atla
    if (!hasSecondary && next == MODE_WEATHER_S) {
        next = (next + 1) % MODE_COUNT;
    }
    return (AppMode)next;
}

extern AppState appState;
