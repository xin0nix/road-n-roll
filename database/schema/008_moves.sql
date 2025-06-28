CREATE TABLE moves (
    move_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    round_id UUID NOT NULL REFERENCES rounds(round_id) ON DELETE CASCADE,
    voter_id UUID NOT NULL REFERENCES game_players(game_player_id),
    chosen_card_id UUID NOT NULL REFERENCES round_cards(round_card_id),
    created_at TIMESTAMP NOT NULL DEFAULT NOW(),
    UNIQUE (round_id, voter_id)
);
