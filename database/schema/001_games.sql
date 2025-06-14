CREATE TABLE game_statuses (
    status_id SERIAL PRIMARY KEY,
    status_name VARCHAR(20) UNIQUE NOT NULL,
    description TEXT
);

INSERT INTO game_statuses (status_name, description) VALUES
    ('pending', 'Ждём игроков'),
    ('active', 'Играем'),
    ('finished', 'Игра завершена');

CREATE TABLE games (
    game_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    status_id INT NOT NULL REFERENCES game_statuses(status_id),
    created_at TIMESTAMP NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_games_status ON games(status_id);
