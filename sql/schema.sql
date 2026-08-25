-- MySQL 5.7+ / MariaDB 10.4+
CREATE TABLE IF NOT EXISTS vehicle_events (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  device_id VARCHAR(64) NOT NULL,
  occurred_at DATETIME NOT NULL,
  direction ENUM('links','rechts') NOT NULL,
  class ENUM('sonstig','pkw','lkw') NOT NULL,
  length_cm SMALLINT UNSIGNED NULL,
  speed_kmh SMALLINT UNSIGNED NULL,
  height_px SMALLINT UNSIGNED NULL,
  width_px SMALLINT UNSIGNED NULL,
  area_px INT UNSIGNED NULL,
  frames SMALLINT UNSIGNED NULL,
  duration_ms INT UNSIGNED NULL,
  confident TINYINT(1) NOT NULL DEFAULT 0,
  received_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  KEY idx_device_time (device_id, occurred_at),
  KEY idx_time_class (occurred_at, class)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Nachklassifikation, Beispiel (vorher Datensicherung erstellen):
-- UPDATE vehicle_events
-- SET class = CASE WHEN length_cm >= 700 THEN 'lkw'
--                  WHEN length_cm >= 220 THEN 'pkw' ELSE 'sonstig' END
-- WHERE device_id = 'cam-zaehler-01' AND confident = 1;
