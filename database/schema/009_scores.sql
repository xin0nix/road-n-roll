CREATE TABLE scores (
    score_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    game_player_id UUID NOT NULL REFERENCES game_players(game_player_id) ON DELETE CASCADE,
    round_id UUID NOT NULL REFERENCES rounds(round_id) ON DELETE CASCADE,
    points INT NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT NOW(),
    UNIQUE (game_player_id, round_id)
);
