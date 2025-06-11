CREATE TABLE player_colors (
    color_id SERIAL PRIMARY KEY,
    color_name VARCHAR(20) NOT NULL UNIQUE,
    color_hex VARCHAR(7) NOT NULL UNIQUE
);

INSERT INTO player_colors (color_name, color_hex) VALUES
    ('Red', '#C0392B'),
    ('Blue', '#3498DB'),
    ('Green', '#1ABC9C'),
    ('Yellow', '#F1C40F'),
    ('Velvet', '#9B59B6'),
    ('Orange', '#E67E22');
