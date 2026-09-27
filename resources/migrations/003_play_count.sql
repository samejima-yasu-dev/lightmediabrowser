ALTER TABLE videos ADD COLUMN play_count INTEGER NOT NULL DEFAULT 0;
CREATE INDEX IF NOT EXISTS idx_videos_play_count ON videos(play_count);