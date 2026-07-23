#pragma once

#include "signal_pets_app.h"

static const PetArchetype PET_DATABASE[] = {
    // --- COMUNE ---
    {"Dino-Byte", "Retro", RarityCommon, 1.0f},
    {"Bit-Kitty", "Cyber", RarityCommon, 1.0f},
    {"Pixel-Pup", "Bio", RarityCommon, 1.0f},
    {"Null-Slime", "Void", RarityCommon, 0.9f},
    {"Volt-Mouse", "Electric", RarityCommon, 1.0f},
    {"RFID-Ant", "Radio", RarityCommon, 0.9f},
    {"Bug-Catcher", "System", RarityCommon, 1.0f},
    {"Pawn-Bot", "Cyber", RarityCommon, 1.1f},

    // --- RARE ---
    {"Cyber-Bot", "Cyber", RarityRare, 1.3f},
    {"Nano-Shark", "Water", RarityRare, 1.4f},
    {"RF-Fox", "Radio", RarityRare, 1.4f},
    {"Logic-Golem", "Earth", RarityRare, 1.3f},
    {"Firewall-Owl", "Fire", RarityRare, 1.3f},
    {"Sub-Giga-Bee", "Radio", RarityRare, 1.4f},
    {"Cache-Snake", "System", RarityRare, 1.3f},
    {"Bandwidth-Bear", "Radio", RarityRare, 1.5f},

    // --- EPICE ---
    {"Glitch-Ghost", "Void", RarityEpic, 1.7f},
    {"Plasma-Dragon", "Fire", RarityEpic, 1.8f},
    {"Crypt-Sentry", "Cyber", RarityEpic, 1.7f},
    {"Quantum-Cat", "Void", RarityEpic, 1.9f},
    {"Infrared-Viper", "Infrared", RarityEpic, 1.7f},
    {"Hydra-Port", "Water", RarityEpic, 1.8f},
    {"Kernel-Kitsune", "System", RarityEpic, 2.0f},

    // --- LEGENDARE ---
    {"Flipper-Prime", "Omni", RarityLegendary, 2.5f},
    {"Zero-Kraken", "Water", RarityLegendary, 2.4f},
    {"Root-Phoenix", "Fire", RarityLegendary, 2.6f},
    {"Spectre-Wolf", "Void", RarityLegendary, 2.3f},
    {"Signal-Titan", "Radio", RarityLegendary, 2.7f},

    // --- MITICE ---
    {"Overclock-God", "Omni", RarityMythic, 3.2f},
    {"Matrix-Architect", "System", RarityMythic, 3.5f}};

#define TOTAL_PETS (sizeof(PET_DATABASE) / sizeof(PetArchetype))

static inline uint32_t calculate_hash(const uint8_t* uid, uint8_t len) {
    uint32_t hash = 5381;
    for(uint8_t i = 0; i < len; i++) {
        hash = ((hash << 5) + hash) + uid[i];
    }
    return hash;
}

static inline const char* get_rarity_str(PetRarity rarity) {
    switch(rarity) {
    case RarityCommon:
        return "[COMUN]";
    case RarityRare:
        return "[RAR]";
    case RarityEpic:
        return "<EPIC>";
    case RarityLegendary:
        return "*LEGENDAR*";
    case RarityMythic:
        return "!!MITIC!!";
    default:
        return "???";
    }
}

static inline void generate_pet(const uint8_t* uid, uint8_t len, Pet* pet) {
    uint32_t hash = calculate_hash(uid, len);
    uint16_t index = hash % TOTAL_PETS;
    const PetArchetype* arch = &PET_DATABASE[index];

    pet->id = (uint16_t)(hash & 0xFFFF);
    pet->rarity = arch->rarity;
    pet->level = (hash % 10) + 1;

    snprintf(pet->name, MAX_NAME_LEN, "%s", arch->name);
    snprintf(pet->element, MAX_ELEMENT_LEN, "%s", arch->element);

    pet->attack = (uint8_t)((((hash & 0xFF) % 40) + 20) * arch->stat_mult);
    pet->defense = (uint8_t)(((((hash >> 8) & 0xFF) % 40) + 15) * arch->stat_mult);
    pet->speed = (uint8_t)(((((hash >> 16) & 0xFF) % 40) + 10) * arch->stat_mult);
}
