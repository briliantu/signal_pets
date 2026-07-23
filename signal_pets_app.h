#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <notification/notification_messages.h>

#define STORAGE_PATH EXT_PATH("apps_data/signal_pets/cyberdex.dat")
#define MAX_NAME_LEN 24
#define MAX_ELEMENT_LEN 12

typedef enum {
    ScreenMain,
    ScreenCyberDex,
    ScreenResetConfirm
} AppScreen;

typedef enum {
    RarityCommon = 0,
    RarityRare,
    RarityEpic,
    RarityLegendary,
    RarityMythic
} PetRarity;

typedef struct {
    const char* name;
    const char* element;
    PetRarity rarity;
    float stat_mult;
} PetArchetype;

typedef struct {
    char name[MAX_NAME_LEN];
    char element[MAX_ELEMENT_LEN];
    PetRarity rarity;
    uint16_t id;
    uint8_t attack;
    uint8_t defense;
    uint8_t speed;
    uint8_t level;
} Pet;

typedef struct {
    Pet pets[100];
    uint16_t count;
} CyberDex;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMutex* mutex;
    NotificationApp* notifications;
    Pet current_pet;
    CyberDex cyberdex;
    bool has_pet;
    
    AppScreen current_screen;
    uint16_t cyberdex_index;
} SignalPetsApp;