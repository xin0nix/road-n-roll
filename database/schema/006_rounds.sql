CREATE TABLE rounds (
    round_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    game_id UUID NOT NULL REFERENCES games(game_id) ON DELETE CASCADE,
    round_number INT NOT NULL,
    narrator_id UUID NOT NULL REFERENCES game_players(game_player_id),
    clue TEXT NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT NOW(),
    UNIQUE(game_id, round_number)
);
