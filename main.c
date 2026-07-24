#include "signal_pets_app.h"
#include "pet_db.h"
#include "storage.h"
#include <nfc/nfc_scanner.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3b/iso14443_3b.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a.h>
#include <nfc/protocols/iso14443_4b/iso14443_4b.h>
#include <nfc/protocols/iso15693_3/iso15693_3.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/mf_plus/mf_plus.h>
#include <nfc/protocols/mf_desfire/mf_desfire.h>
#include <nfc/protocols/felica/felica.h>
#include <nfc/protocols/slix/slix.h>
#include <nfc/protocols/st25tb/st25tb.h>

typedef struct {
    SignalPetsApp* app;
    bool detected;
    NfcProtocol protocol;
} NfcScanContext;

static void render_cyberdex_screen(Canvas* canvas, SignalPetsApp* app) {
    if(app->cyberdex.count == 0) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 35, 20, "CyberDex");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 12, 40, "Niciun SignalPet colectat!");
        canvas_draw_str(canvas, 25, 60, "[Back] Înapoi");
        return;
    }

    Pet* pet = &app->cyberdex.pets[app->cyberdex_index];

    canvas_set_font(canvas, FontSecondary);
    char header_buf[32];
    snprintf(
        header_buf,
        sizeof(header_buf),
        "CyberDex #%d/%d",
        app->cyberdex_index + 1,
        app->cyberdex.count);
    canvas_draw_str(canvas, 2, 10, header_buf);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 22, pet->name);

    canvas_set_font(canvas, FontSecondary);
    char meta_buf[40];
    snprintf(
        meta_buf,
        sizeof(meta_buf),
        "Lv.%d | %s | %s",
        pet->level,
        pet->element,
        get_rarity_str(pet->rarity));
    canvas_draw_str(canvas, 2, 33, meta_buf);

    canvas_draw_line(canvas, 0, 36, 128, 36);

    char stat_buf[32];
    snprintf(
        stat_buf,
        sizeof(stat_buf),
        "ATK:%d  DEF:%d  SPD:%d",
        pet->attack,
        pet->defense,
        pet->speed);
    canvas_draw_str(canvas, 2, 48, stat_buf);

    canvas_draw_str(canvas, 2, 60, "< / > Navigare");
    canvas_draw_str(canvas, 80, 60, "[Back] Ieșire");
}

static void render_reset_confirm_screen(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 15, 18, "RESET CYBERDEX?");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 5, 33, "Toate pet-urile salvate");
    canvas_draw_str(canvas, 5, 43, "vor fi șterse definitiv!");

    canvas_draw_line(canvas, 0, 48, 128, 48);

    canvas_draw_str(canvas, 5, 60, "[OK] Da (Șterge)");
    canvas_draw_str(canvas, 80, 60, "[Back] Nu");
}

static void render_callback(Canvas* canvas, void* ctx) {
    SignalPetsApp* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    canvas_clear(canvas);

    if(app->current_screen == ScreenResetConfirm) {
        render_reset_confirm_screen(canvas);
    } else if(app->current_screen == ScreenCyberDex) {
        render_cyberdex_screen(canvas, app);
    } else {
        if(!app->has_pet) {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 18, 8, "SignalPets");

            canvas_set_font(canvas, FontSecondary);
            if(app->scanning) {
                canvas_draw_str(canvas, 2, 26, "Scanează...");
                canvas_draw_str(canvas, 2, 38, "Aproape gata...");
                canvas_draw_str(canvas, 2, 52, "[Back] Anulează");
            } else {
                canvas_draw_str(canvas, 2, 26, "Scanează un tag NFC!");
                char count_str[32];
                snprintf(count_str, sizeof(count_str), "CyberDex: %d/100", app->cyberdex.count);
                canvas_draw_str(canvas, 2, 40, count_str);
                if(app->scan_failed) {
                    canvas_draw_str(canvas, 2, 52, "Scanare eșuată");
                }
                canvas_draw_str(canvas, 2, 60, "[OK]Scan [v]Dex [Hold v]Reset");
            }
        } else {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 2, 12, app->current_pet.name);

            canvas_set_font(canvas, FontSecondary);
            char meta_str[40];
            snprintf(
                meta_str,
                sizeof(meta_str),
                "Lv.%d | %s | %s",
                app->current_pet.level,
                app->current_pet.element,
                get_rarity_str(app->current_pet.rarity));
            canvas_draw_str(canvas, 2, 20, meta_str);

            canvas_draw_line(canvas, 0, 30, 128, 30);

            char stat_buf[32];
            snprintf(
                stat_buf,
                sizeof(stat_buf),
                "ATK:%d  DEF:%d  SPD:%d",
                app->current_pet.attack,
                app->current_pet.defense,
                app->current_pet.speed);
            canvas_draw_str(canvas, 2, 40, stat_buf);

            if(app->scanning) {
                canvas_draw_str(canvas, 2, 52, "Scanează... [Back] Anulează");
            } else {
                canvas_draw_str(canvas, 2, 56, "[OK]Scan [v]Dex [Hold v]Reset");
            }
        }
    }

    furi_mutex_release(app->mutex);
}

static void trigger_feedback(SignalPetsApp* app, PetRarity rarity) {
    if(rarity >= RarityLegendary) {
        notification_message(app->notifications, &sequence_success);
    } else {
        notification_message(app->notifications, &sequence_single_vibro);
    }
}

static void nfc_scanner_callback(NfcScannerEvent event, void* context) {
    NfcScanContext* scan_ctx = context;
    if(event.type != NfcScannerEventTypeDetected) {
        return;
    }

    if(event.data.protocol_num > 0) {
        scan_ctx->protocol = event.data.protocols[0];
        scan_ctx->detected = true;
    }
}

static const uint8_t*
    get_device_uid(NfcProtocol protocol, const NfcDeviceData* device_data, size_t* uid_len) {
    switch(protocol) {
    case NfcProtocolIso14443_3a:
        return iso14443_3a_get_uid((const Iso14443_3aData*)device_data, uid_len);
    case NfcProtocolIso14443_3b:
        return iso14443_3b_get_uid((const Iso14443_3bData*)device_data, uid_len);
    case NfcProtocolIso14443_4a:
        return iso14443_4a_get_uid((const Iso14443_4aData*)device_data, uid_len);
    case NfcProtocolIso14443_4b:
        return iso14443_4b_get_uid((const Iso14443_4bData*)device_data, uid_len);
    case NfcProtocolIso15693_3:
        return iso15693_3_get_uid((const Iso15693_3Data*)device_data, uid_len);
    case NfcProtocolMfUltralight:
        return mf_ultralight_get_uid((const MfUltralightData*)device_data, uid_len);
    case NfcProtocolMfClassic:
        return mf_classic_get_uid((const MfClassicData*)device_data, uid_len);
    case NfcProtocolMfPlus:
        return mf_plus_get_uid((const MfPlusData*)device_data, uid_len);
    case NfcProtocolMfDesfire:
        return mf_desfire_get_uid((const MfDesfireData*)device_data, uid_len);
    case NfcProtocolFelica:
        return felica_get_uid((const FelicaData*)device_data, uid_len);
    case NfcProtocolSlix:
        return slix_get_uid((const SlixData*)device_data, uid_len);
    case NfcProtocolSt25tb:
        return st25tb_get_uid((const St25tbData*)device_data, uid_len);
    default:
        *uid_len = 0;
        return NULL;
    }
}

static int32_t scan_thread_callback(void* context) {
    SignalPetsApp* app = context;
    Nfc* nfc = nfc_alloc();
    if(!nfc) {
        goto cleanup;
    }

    NfcScanner* scanner = nfc_scanner_alloc(nfc);
    if(!scanner) {
        nfc_free(nfc);
        goto cleanup;
    }

    NfcScanContext scan_ctx = {.app = app, .detected = false, .protocol = NfcProtocolInvalid};
    nfc_scanner_start(scanner, nfc_scanner_callback, &scan_ctx);

    uint32_t start_time = furi_get_tick();
    while(!scan_ctx.detected) {
        bool cancel_requested = false;
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        cancel_requested = app->scan_cancel_requested;
        furi_mutex_release(app->mutex);
        if(cancel_requested || (furi_get_tick() - start_time >= 3000)) {
            break;
        }
        furi_delay_ms(50);
    }

    nfc_scanner_stop(scanner);
    nfc_scanner_free(scanner);

    bool cancel_requested = false;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    cancel_requested = app->scan_cancel_requested;
    furi_mutex_release(app->mutex);

    if(scan_ctx.detected && scan_ctx.protocol != NfcProtocolInvalid && !cancel_requested) {
        NfcPoller* poller = nfc_poller_alloc(nfc, scan_ctx.protocol);
        if(poller) {
            if(nfc_poller_detect(poller)) {
                const NfcDeviceData* device_data = nfc_poller_get_data(poller);
                if(device_data) {
                    size_t uid_len = 0;
                    const uint8_t* uid = get_device_uid(scan_ctx.protocol, device_data, &uid_len);
                    if(uid && uid_len > 0) {
                        uint8_t uid_len8 = uid_len > 255 ? 255 : (uint8_t)uid_len;
                        Pet new_pet;
                        generate_pet(uid, uid_len8, &new_pet);

                        furi_mutex_acquire(app->mutex, FuriWaitForever);
                        app->current_pet = new_pet;
                        app->has_pet = true;
                        cyberdex_add_pet(&app->cyberdex, &new_pet);
                        app->scan_failed = false;
                        furi_mutex_release(app->mutex);

                        trigger_feedback(app, new_pet.rarity);
                    }
                }
            }
            nfc_poller_free(poller);
        }
    } else if(!scan_ctx.detected && !cancel_requested) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        app->scan_failed = true;
        furi_mutex_release(app->mutex);
    }

    nfc_free(nfc);

cleanup:
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->scanning = false;
    app->scan_cancel_requested = false;
    furi_mutex_release(app->mutex);
    view_port_update(app->view_port);

    return 0;
}

static void start_nfc_scan(SignalPetsApp* app) {
    if(app->scanning) {
        return;
    }

    if(app->scan_thread && furi_thread_get_state(app->scan_thread) != FuriThreadStateStopped) {
        return;
    }

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->scanning = true;
    app->scan_failed = false;
    app->scan_cancel_requested = false;
    furi_mutex_release(app->mutex);

    view_port_update(app->view_port);
    furi_thread_start(app->scan_thread);
}

static void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

int32_t signal_pets_app_main(void* p) {
    UNUSED(p);

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    SignalPetsApp* app = malloc(sizeof(SignalPetsApp));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->has_pet = false;
    app->scanning = false;
    app->scan_failed = false;
    app->scan_cancel_requested = false;
    app->scan_thread = furi_thread_alloc_ex("scan", 4096, scan_thread_callback, app);
    app->current_screen = ScreenMain;
    app->cyberdex_index = 0;

    cyberdex_load(&app->cyberdex);

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, render_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, event_queue);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    InputEvent event;
    bool running = true;

    while(running) {
        if(furi_message_queue_get(event_queue, &event, 100) == FuriStatusOk) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);

            if(app->current_screen == ScreenMain) {
                if(event.type == InputTypeShort) {
                    if(event.key == InputKeyBack) {
                        if(app->scanning) {
                            furi_mutex_acquire(app->mutex, FuriWaitForever);
                            app->scan_cancel_requested = true;
                            furi_mutex_release(app->mutex);
                        } else {
                            running = false;
                        }
                    } else if(event.key == InputKeyOk) {
                        if(!app->scanning) {
                            start_nfc_scan(app);
                        }
                    } else if(event.key == InputKeyDown) {
                        if(!app->scanning) {
                            app->current_screen = ScreenCyberDex;
                            app->cyberdex_index = 0;
                        }
                    }
                } else if(event.type == InputTypeLong && event.key == InputKeyDown) {
                    if(!app->scanning) {
                        app->current_screen = ScreenResetConfirm;
                    }
                }
            } else if(app->current_screen == ScreenCyberDex) {
                if(event.type == InputTypeShort) {
                    if(event.key == InputKeyBack) {
                        app->current_screen = ScreenMain;
                    } else if(event.key == InputKeyRight && app->cyberdex.count > 0) {
                        app->cyberdex_index = (app->cyberdex_index + 1) % app->cyberdex.count;
                    } else if(event.key == InputKeyLeft && app->cyberdex.count > 0) {
                        app->cyberdex_index = (app->cyberdex_index == 0) ?
                                                  (app->cyberdex.count - 1) :
                                                  (app->cyberdex_index - 1);
                    }
                } else if(event.type == InputTypeLong && event.key == InputKeyDown) {
                    app->current_screen = ScreenResetConfirm;
                }
            } else if(app->current_screen == ScreenResetConfirm) {
                if(event.type == InputTypeShort) {
                    if(event.key == InputKeyOk) {
                        cyberdex_reset(&app->cyberdex);
                        notification_message(app->notifications, &sequence_single_vibro);
                        app->current_screen = ScreenMain;
                    } else if(event.key == InputKeyBack) {
                        app->current_screen = ScreenMain;
                    }
                }
            }

            furi_mutex_release(app->mutex);
            view_port_update(app->view_port);
        }
    }

    if(app->scan_thread) {
        if(furi_thread_get_state(app->scan_thread) == FuriThreadStateRunning) {
            furi_thread_join(app->scan_thread);
        }
        furi_thread_free(app->scan_thread);
    }

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_mutex_free(app->mutex);
    furi_message_queue_free(event_queue);
    free(app);

    return 0;
}
