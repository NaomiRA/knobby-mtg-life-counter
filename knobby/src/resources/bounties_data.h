#ifndef KNOBBY_BOUNTIES_DATA_H
#define KNOBBY_BOUNTIES_DATA_H

typedef struct {
    const char *name;
    const char *oracle_text;
} bounty_card_t;

#define BOUNTY_CARD_COUNT 12
static const bounty_card_t bounty_cards[BOUNTY_CARD_COUNT] = {
    {"Eriana, Wrecking Ball", "At the beginning of your end step, if you committed a crime this turn, collect your reward. (Targeting opponents, anything they control, and/or cards in their graveyards is a crime.)"},
    {"Frankie The Fang", "At the beginning of your end step, if the player with the most life or tied for most life as the turn began was dealt combat damage this turn, collect your reward."},
    {"Gorra Tash and Silas", "At the beginning of your end step, if all opponents were dealt combat damage this turn, collect your reward."},
    {"Lord Fajjal", "At the beginning of your end step, if a land card was put into your graveyard from anywhere this turn, collect your reward."},
    {"Lyssa, Sterling Collector", "At the beginning of your end step, if the player with the most cards in hand or tied for most cards as the turn began was dealt combat damage this turn, collect your reward."},
    {"Miron Tillas Jr.", "At the beginning of your end step, if the nearest opponent to your left as the turn began was dealt combat damage this turn, collect your reward."},
    {"Paq, Fleeting Filcher", "At the beginning of your end step, if there are four or more card types among cards in your graveyard, collect your reward."},
    {"Rissa \"Blades\" Lee", "At the beginning of your end step, if you cast an instant or sorcery spell this turn while you had two or more instant and/or sorcery cards in your graveyard, collect your reward."},
    {"Sleepy Sovka", "At the beginning of your end step, if you cast no spells this turn, collect your reward."},
    {"Squeakers the Sly", "At the beginning of your end step, if a legendary creature you controlled attacked alone this turn, collect your reward."},
    {"The Outsider", "At the beginning of your end step, if you cast a spell from anywhere other than your hand this turn, collect your reward."},
    {"Vara Beth Hannifer", "At the beginning of your end step, if the nearest opponent to your right as the turn began was dealt combat damage this turn, collect your reward."},
};

#endif
