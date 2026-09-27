ALTER TABLE videos ADD COLUMN is_favorite INTEGER NOT NULL DEFAULT 0;
CREATE INDEX IF NOT EXISTS idx_videos_favorite ON videos(is_favorite);