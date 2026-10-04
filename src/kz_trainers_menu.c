#include "kz.h"
#include "settings.h"

enum trainer {
    TRAINER_CHEST_RI,
};

struct trainer_item {
    enum trainer    id;
    char           *name;
};

static struct trainer_item trainer_table[] = {
    { TRAINER_CHEST_RI, "chest minigame ri" },
};

static int trainer_event(event_handler_t *handler, menu_event_t event, void **event_data){
    enum trainer trainer = (enum trainer)handler->callback_data;
    if(event == MENU_EVENT_ACTIVATE){
        switch(trainer){
            case TRAINER_CHEST_RI:
            settings->chest_ri = !settings->chest_ri;
            break;
        }
    }else if(event == MENU_EVENT_UPDATE){
        menu_item_t *item = handler->subscriber;
        switch(trainer){
            case TRAINER_CHEST_RI:
            menu_checkbox_set(item, settings->chest_ri);
            break;
        }
    }
    return 1;
}

menu_t *create_trainers_menu(void){
    static menu_t trainers;
    menu_init(&trainers, 0, 0);
    menu_padding_set(&trainers, 0, 2);
    trainers.selected_item = menu_button_add(&trainers, 0, 0, "return", menu_return, NULL);
    for(int i = 0;i < sizeof(trainer_table) / sizeof(*trainer_table);i++){
        menu_item_t *item = menu_checkbox_add(&trainers, 0, i + 1);
        menu_item_register_event(item, MENU_EVENT_ACTIVATE | MENU_EVENT_UPDATE, trainer_event, (void*)trainer_table[i].id);
        menu_label_add(&trainers, 2, i + 1, trainer_table[i].name);
    }
    return &trainers;
}
