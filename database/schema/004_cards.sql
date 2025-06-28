CREATE TABLE cards (
    card_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    image_url TEXT UNIQUE NOT NULL
);
