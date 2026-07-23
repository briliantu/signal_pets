#include "signal_pets_app.h"
#include "pet_db.h"
#include "storage.h"

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
    snprintf(header_buf, sizeof(header_buf), "CyberDex #%d/%d", app->cyberdex_index + 1, app->cyberdex.count);
    canvas_draw_str(canvas, 2, 10, header_buf);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 22, pet->name);

    canvas_set_font(canvas, FontSecondary);
    char meta_buf[40];
    snprintf(meta_buf, sizeof(meta_buf), "Lv.%d | %s | %s", pet->level, pet->element, get_rarity_str(pet->rarity));
    canvas_draw_str(canvas, 2, 33, meta_buf);

    canvas_draw_line(canvas, 0, 36, 128, 36);

    char stat_buf[32];
    snprintf(stat_buf, sizeof(stat_buf), "ATK:%d  DEF:%d  SPD:%d", pet->attack, pet->defense, pet->speed);
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
            canvas_draw_str(canvas, 28, 15, "SignalPets");
            
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 8, 30, "Scanează un tag NFC!");
            
            char count_str[32];
            snprintf(count_str, sizeof(count_str), "CyberDex: %d/100", app->cyberdex.count);
            canvas_draw_str(canvas, 24, 43, count_str);
            
            canvas_draw_str(canvas, 2, 60, "[OK]Scan | [v]Dex | [Hold v]Reset");
        } else {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 2, 12, app->current_pet.name);

            canvas_set_font(canvas, FontSecondary);
            char meta_str[40];
            snprintf(meta_str, sizeof(meta_str), "Lv.%d | %s | %s", 
                     app->current_pet.level, 
                     app->current_pet.element, 
                     get_rarity_str(app->current_pet.rarity));
            canvas_draw_str(canvas, 2, 23, meta_str);

            canvas_draw_line(canvas, 0, 26, 128, 26);

            char stat_buf[32];
            snprintf(stat_buf, sizeof(stat_buf), "ATK:%d  DEF:%d  SPD:%d", 
                     app->current_pet.attack, app->current_pet.defense, app->current_pet.speed);
            canvas_draw_str(canvas, 2, 40, stat_buf);

            canvas_draw_str(canvas, 2, 60, "[OK]Scan | [v]Dex | [Hold v]Reset");
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

static void scan_nfc_card(SignalPetsApp* app) {
    Nfc* nfc = nfc_alloc();
    NfcDevice* device = nfc_device_alloc();
    NfcPoller* poller = nfc_poller_alloc(nfc, NfcProtocolIso14443_3a);

    if(nfc_poller_start(poller, NULL, NULL)) {
        size_t uid_len = 0;
        const uint8_t* uid = nfc_device_get_uid(device, &uid_len);

        if(uid_len > 0) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            
            Pet new_pet;
            generate_pet(uid, uid_len, &new_pet);
            app->current_pet = new_pet;
            app->has_pet = true;

            cyberdex_add_pet(&app->cyberdex, &new_pet);
            trigger_feedback(app, new_pet.rarity);
            
            furi_mutex_release(app->mutex);
        }
    }

    nfc_poller_stop(poller);
    nfc_poller_free(poller);
    nfc_device_free(device);
    nfc_free(nfc);
}

int32_t signal_pets_app_main(void* p) {
    UNUSED(p);

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    SignalPetsApp* app = malloc(sizeof(SignalPetsApp));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->has_pet = false;
    app->current_screen = ScreenMain;
    app->cyberdex_index = 0;

    cyberdex_load(&app->cyberdex);

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, render_callback, app);
    view_port_input_callback_set(app->view_port, (ViewPortInputCallback)furi_message_queue_put, event_queue);

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
                        running = false;
                    } else if(event.key == InputKeyOk) {
                        scan_nfc_card(app);
                    } else if(event.key == InputKeyDown) {
                        app->current_screen = ScreenCyberDex;
                        app->cyberdex_index = 0;
                    }
                } else if(event.type == InputTypeLong && event.key == InputKeyDown) {
                    app->current_screen = ScreenResetConfirm;
                }
            } else if(app->current_screen == ScreenCyberDex) {
                if(event.type == InputTypeShort) {
                    if(event.key == InputKeyBack) {
                        app->current_screen = ScreenMain;
                    } else if(event.key == InputKeyRight && app->cyberdex.count > 0) {
                        app->cyberdex_index = (app->cyberdex_index + 1) % app->cyberdex.count;
                    } else if(event.key == InputKeyLeft && app->cyberdex.count > 0) {
                        app->cyberdex_index = (app->cyberdex_index == 0) ? 
                            (app->cyberdex.count - 1) : (app->cyberdex_index - 1);
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

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_mutex_free(app->mutex);
    furi_message_queue_free(event_queue);
    free(app);

    return 0;
}