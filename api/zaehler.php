<?php
declare(strict_types=1);
require __DIR__ . '/config.php';

header('Content-Type: application/json; charset=utf-8');
header('X-Content-Type-Options: nosniff');
header('Cache-Control: no-store');

function reply(int $status, array $body): never {
    http_response_code($status);
    echo json_encode($body, JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE);
    exit;
}
function db(): PDO {
    static $pdo;
    return $pdo ??= new PDO(DB_DSN, DB_USER, DB_PASSWORD, [
        PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
        PDO::ATTR_EMULATE_PREPARES => false,
        PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
    ]);
}
function apiAuth(): void {
    $provided = (string)($_SERVER['HTTP_X_API_KEY'] ?? '');
    if ($provided === '' || !hash_equals(API_KEY, $provided)) reply(401, ['error' => 'unauthorized']);
}
function cleanString(mixed $value, int $max): string {
    return is_string($value) && preg_match('/^[A-Za-z0-9_-]{1,' . $max . '}$/', $value) ? $value : '';
}

try {
    if ($_SERVER['REQUEST_METHOD'] === 'POST') {
        apiAuth();
        if ((int)($_SERVER['CONTENT_LENGTH'] ?? 0) > 4096) reply(413, ['error' => 'payload too large']);
        $data = json_decode(file_get_contents('php://input'), true, 32, JSON_THROW_ON_ERROR);
        if (!is_array($data)) reply(400, ['error' => 'invalid JSON']);
        $device = cleanString($data['device_id'] ?? null, 64);
        $direction = $data['richtung'] ?? null;
        $class = $data['klasse'] ?? null;
        $time = DateTimeImmutable::createFromFormat('!Y-m-d\\TH:i:s', (string)($data['zeit'] ?? ''));
        if ($device === '' || !in_array($direction, ['links', 'rechts'], true) || !in_array($class, ['sonstig', 'pkw', 'lkw'], true) || !$time) {
            reply(422, ['error' => 'invalid event']);
        }
        $number = static function (mixed $v, int $max): ?int {
            return filter_var($v, FILTER_VALIDATE_INT, ['options' => ['min_range' => 0, 'max_range' => $max]]) !== false ? (int)$v : null;
        };
        $values = [$number($data['laenge_cm'] ?? null, 3000), $number($data['tempo_kmh'] ?? null, 300), $number($data['hoehe_px'] ?? null, 1200), $number($data['breite_px'] ?? null, 1600), $number($data['flaeche'] ?? null, 2000000), $number($data['frames'] ?? null, 1000), $number($data['dauer_ms'] ?? null, 120000)];
        $stmt = db()->prepare('INSERT INTO vehicle_events (device_id, occurred_at, direction, class, length_cm, speed_kmh, height_px, width_px, area_px, frames, duration_ms, confident) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)');
        $stmt->execute([$device, $time->format('Y-m-d H:i:s'), $direction, $class, ...$values, !empty($data['sicher']) ? 1 : 0]);
        reply(201, ['ok' => true]);
    }
    if ($_SERVER['REQUEST_METHOD'] !== 'GET') reply(405, ['error' => 'method not allowed']);
    apiAuth();
    $period = $_GET['stats'] ?? 'stunden';
    $device = isset($_GET['device']) ? cleanString($_GET['device'], 64) : '';
    if (!in_array($period, ['stunden', 'woche'], true)) reply(422, ['error' => 'invalid stats range']);
    $from = $period === 'stunden' ? '-24 hours' : '-7 days';
    $sql = 'SELECT DATE_FORMAT(occurred_at, ?) AS bucket, direction, class, COUNT(*) AS count, ROUND(AVG(speed_kmh), 1) AS avg_speed FROM vehicle_events WHERE occurred_at >= DATE_ADD(NOW(), INTERVAL ' . ($period === 'stunden' ? '24 HOUR' : '7 DAY') . ')';
    $args = [$period === 'stunden' ? '%Y-%m-%d %H:00' : '%Y-%m-%d'];
    if ($device !== '') { $sql .= ' AND device_id = ?'; $args[] = $device; }
    $sql .= ' GROUP BY bucket, direction, class ORDER BY bucket';
    $stmt = db()->prepare($sql); $stmt->execute($args);
    reply(200, ['data' => $stmt->fetchAll()]);
} catch (JsonException) { reply(400, ['error' => 'invalid JSON']); }
catch (PDOException $e) { error_log('vehicle API database error: ' . $e->getMessage()); reply(500, ['error' => 'server error']); }
