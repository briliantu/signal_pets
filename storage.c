#include "storage.h"
#include <storage/storage.h>

void cyberdex_save(CyberDex* cyberdex) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_migrate(
        storage, EXT_PATH("apps_data/signal_pets"), EXT_PATH("apps_data/signal_pets"));

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, STORAGE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, cyberdex, sizeof(CyberDex));
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void cyberdex_load(CyberDex* cyberdex) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    cyberdex->count = 0;
    if(storage_file_open(file, STORAGE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_read(file, cyberdex, sizeof(CyberDex));
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

bool cyberdex_add_pet(CyberDex* cyberdex, const Pet* pet) {
    for(uint16_t i = 0; i < cyberdex->count; i++) {
        if(cyberdex->pets[i].id == pet->id) {
            return false;
        }
    }

    if(cyberdex->count < 100) {
        cyberdex->pets[cyberdex->count] = *pet;
        cyberdex->count++;
        cyberdex_save(cyberdex);
        return true;
    }

    return false;
}

void cyberdex_reset(CyberDex* cyberdex) {
    cyberdex->count = 0;
    memset(cyberdex->pets, 0, sizeof(cyberdex->pets));

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage_common_stat(storage, STORAGE_PATH, NULL) == FSE_OK) {
        storage_common_remove(storage, STORAGE_PATH);
    }
    furi_record_close(RECORD_STORAGE);
}
