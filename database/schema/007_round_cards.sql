CREATE TABLE round_cards (
    round_card_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    round_id UUID NOT NULL REFERENCES rounds(round_id) ON DELETE CASCADE,
    game_player_id UUID NOT NULL REFERENCES game_players(game_player_id),
    card_id UUID NOT NULL REFERENCES cards(card_id),
    played_at TIMESTAMP NOT NULL DEFAULT NOW(),
    UNIQUE (round_id, game_player_id)
);
