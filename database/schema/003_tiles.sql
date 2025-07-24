CREATE TABLE tile_type (
    id SERIAL PRIMARY KEY,
    name VARCHAR(50) NOT NULL,
    image_data BYTEA NOT NULL,
    amount INT NOT NULL,
    description TEXT
);

CREATE TABLE layer (
    id SERIAL PRIMARY KEY,
    name VARCHAR(50) NOT NULL
);

CREATE TABLE tile_joint_type (
    id SERIAL PRIMARY KEY,
    place VARCHAR(10) NOT NULL CHECK (place IN ('top', 'right', 'bottom', 'left', 'middle'))
);

CREATE TABLE tile_joint (
    id SERIAL PRIMARY KEY,
    tile_type_id INT NOT NULL,
    joint_type_id INT NOT NULL,
    layer_id INT NOT NULL,
    FOREIGN KEY (tile_type_id) REFERENCES tile_type(id),
    FOREIGN KEY (joint_type_id) REFERENCES tile_joint_type(id),
    FOREIGN KEY (layer_id) REFERENCES layer(id),
    UNIQUE (tile_type_id, joint_type_id, layer_id)
);

CREATE TABLE tile_instance (
    id SERIAL PRIMARY KEY,
    tile_type_id INT NOT NULL,
    game_id INT NOT NULL,
    x INT NOT NULL,
    y INT NOT NULL,
    rotation INT NOT NULL CHECK (rotation IN (0, 90, 180, 270)),
    FOREIGN KEY (tile_type_id) REFERENCES tile_type(id),
    FOREIGN KEY (game_id) REFERENCES games(id)
);
