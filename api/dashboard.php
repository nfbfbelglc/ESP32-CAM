<?php
declare(strict_types=1);
require __DIR__ . '/config.php';
header('Content-Type: text/html; charset=utf-8');
header('X-Frame-Options: DENY'); header('X-Content-Type-Options: nosniff');
try {
  $pdo = new PDO(DB_DSN, DB_USER, DB_PASSWORD, [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION, PDO::ATTR_EMULATE_PREPARES => false]);
  $rows = $pdo->query("SELECT device_id, direction, class, COUNT(*) count, ROUND(AVG(speed_kmh),1) speed FROM vehicle_events WHERE occurred_at >= DATE_SUB(NOW(), INTERVAL 7 DAY) GROUP BY device_id,direction,class ORDER BY device_id,direction,class")->fetchAll(PDO::FETCH_ASSOC);
} catch (PDOException $e) { http_response_code(500); exit('Datenbank nicht verfügbar.'); }
?><!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Autozähler</title><style>body{font:16px system-ui;margin:2rem;color:#17202a}table{border-collapse:collapse;width:100%;max-width:800px}th,td{padding:.65rem;border-bottom:1px solid #ddd;text-align:left}th{background:#f4f6f7}</style><h1>Autozähler · letzte 7 Tage</h1><table><tr><th>Gerät</th><th>Richtung</th><th>Klasse</th><th>Anzahl</th><th>Ø km/h</th></tr><?php foreach($rows as $r): ?><tr><td><?=htmlspecialchars($r['device_id'],ENT_QUOTES,'UTF-8')?></td><td><?=htmlspecialchars($r['direction'],ENT_QUOTES,'UTF-8')?></td><td><?=htmlspecialchars($r['class'],ENT_QUOTES,'UTF-8')?></td><td><?=htmlspecialchars($r['count'],ENT_QUOTES,'UTF-8')?></td><td><?=htmlspecialchars($r['speed'] ?? '–',ENT_QUOTES,'UTF-8')?></td></tr><?php endforeach ?></table>
