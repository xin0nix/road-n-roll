CREATE TABLE game_players (
    game_player_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    game_id UUID NOT NULL REFERENCES games(game_id) ON DELETE CASCADE,
    player_id UUID NOT NULL REFERENCES players(player_id),
    color_id INT NOT NULL REFERENCES player_colors(color_id),
    score INT DEFAULT 0,
    is_host BOOLEAN DEFAULT FALSE,
    joined_at TIMESTAMP NOT NULL DEFAULT NOW(),
    UNIQUE (game_id, player_id),
    UNIQUE (game_id, color_id)
);
