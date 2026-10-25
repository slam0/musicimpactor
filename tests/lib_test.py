#!/usr/bin/env python3
"""Testy mi_lib.c na sztucznych bazach MediaLibrary.sqlitedb (kilka wariantów schematu)."""
import os, re, shutil, sqlite3, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
CLI = os.path.join(HERE, "lib_cli")
errors = 0

def check(cond, msg):
    global errors
    if not cond:
        errors += 1
        print("  BŁĄD:", msg)

SCHEMA_FULL = """
CREATE TABLE item (pid INTEGER PRIMARY KEY, revision_level INTEGER NOT NULL DEFAULT 0, media_type INTEGER NOT NULL DEFAULT 8,
  title_order INTEGER NOT NULL DEFAULT 0, title_order_section INTEGER NOT NULL DEFAULT 0,
  item_artist_pid INTEGER NOT NULL DEFAULT 0, item_artist_order INTEGER NOT NULL DEFAULT 0, item_artist_order_section INTEGER NOT NULL DEFAULT 0,
  album_pid INTEGER NOT NULL DEFAULT 0, album_order INTEGER NOT NULL DEFAULT 0, album_order_section INTEGER NOT NULL DEFAULT 0,
  album_artist_pid INTEGER NOT NULL DEFAULT 0, album_artist_order INTEGER NOT NULL DEFAULT 0, album_artist_order_section INTEGER NOT NULL DEFAULT 0,
  composer_pid INTEGER NOT NULL DEFAULT 0, composer_order INTEGER NOT NULL DEFAULT 0, composer_order_section INTEGER NOT NULL DEFAULT 0,
  genre_id INTEGER NOT NULL DEFAULT 0, genre_order INTEGER NOT NULL DEFAULT 0, genre_order_section INTEGER NOT NULL DEFAULT 0,
  disc_number INTEGER NOT NULL DEFAULT 0, track_number INTEGER NOT NULL DEFAULT 0, base_location_id INTEGER NOT NULL DEFAULT 0,
  remote_location_id INTEGER NOT NULL DEFAULT 0, exclude_from_shuffle INTEGER NOT NULL DEFAULT 0, keep_local INTEGER NOT NULL DEFAULT 0,
  in_my_library INTEGER NOT NULL DEFAULT 1, is_compilation INTEGER NOT NULL DEFAULT 0, date_added INTEGER NOT NULL DEFAULT 0,
  date_downloaded INTEGER NOT NULL DEFAULT 0, download_source_container_pid INTEGER NOT NULL DEFAULT 0);
CREATE TABLE item_extra (item_pid INTEGER PRIMARY KEY, title TEXT, sort_title TEXT, disc_count INTEGER NOT NULL DEFAULT 0,
  track_count INTEGER NOT NULL DEFAULT 0, total_time_ms REAL NOT NULL DEFAULT 0, year INTEGER NOT NULL DEFAULT 0, location TEXT NOT NULL DEFAULT '',
  file_size INTEGER NOT NULL DEFAULT 0, integrity BLOB, is_audible_audio_book INTEGER NOT NULL DEFAULT 0, date_modified INTEGER NOT NULL DEFAULT 0,
  media_kind INTEGER NOT NULL DEFAULT 1, content_rating INTEGER NOT NULL DEFAULT 0, is_user_disabled INTEGER NOT NULL DEFAULT 0,
  bpm INTEGER NOT NULL DEFAULT 0, genius_id INTEGER NOT NULL DEFAULT 0, comment TEXT, grouping TEXT, description TEXT, pending_delete INTEGER NOT NULL DEFAULT 0);
CREATE TABLE item_playback (item_pid INTEGER PRIMARY KEY, audio_format INTEGER NOT NULL DEFAULT 0, bit_rate INTEGER NOT NULL DEFAULT 0,
  codec_type INTEGER NOT NULL DEFAULT 0, codec_subtype INTEGER NOT NULL DEFAULT 0, data_kind INTEGER NOT NULL DEFAULT 0, data_url TEXT,
  duration INTEGER NOT NULL DEFAULT 0, eq_preset TEXT, format INTEGER NOT NULL DEFAULT 0, has_video INTEGER NOT NULL DEFAULT 0,
  relative_volume INTEGER, sample_rate REAL NOT NULL DEFAULT 0, start_time_ms REAL NOT NULL DEFAULT 0, stop_time_ms REAL NOT NULL DEFAULT 0,
  volume_normalization_energy INTEGER NOT NULL DEFAULT 0, gapless_heuristic_info INTEGER NOT NULL DEFAULT 0);
CREATE TABLE item_stats (item_pid INTEGER PRIMARY KEY, user_rating INTEGER NOT NULL DEFAULT 0, needs_restore INTEGER NOT NULL DEFAULT 0,
  play_count_user INTEGER NOT NULL DEFAULT 0, play_count_recent INTEGER NOT NULL DEFAULT 0, has_been_played INTEGER NOT NULL DEFAULT 0,
  date_played INTEGER NOT NULL DEFAULT 0, skip_count_user INTEGER NOT NULL DEFAULT 0);
CREATE TABLE item_store (item_pid INTEGER PRIMARY KEY, store_item_id INTEGER NOT NULL, store_saga_id INTEGER NOT NULL DEFAULT 0, purchase_history_id INTEGER);
CREATE TABLE item_video (item_pid INTEGER PRIMARY KEY, video_quality INTEGER NOT NULL DEFAULT 0);
CREATE TABLE artist (pid INTEGER PRIMARY KEY, name TEXT NOT NULL, name_order INTEGER NOT NULL DEFAULT 0, name_order_section INTEGER NOT NULL DEFAULT 0,
  sort_name TEXT, representative_item_pid INTEGER NOT NULL DEFAULT 0, keep_local INTEGER NOT NULL DEFAULT 0);
CREATE TABLE album_artist (pid INTEGER PRIMARY KEY, name TEXT NOT NULL, name_order INTEGER NOT NULL DEFAULT 0, name_order_section INTEGER NOT NULL DEFAULT 0,
  sort_name TEXT, representative_item_pid INTEGER NOT NULL DEFAULT 0);
CREATE TABLE album (pid INTEGER PRIMARY KEY, name TEXT NOT NULL, name_order INTEGER NOT NULL DEFAULT 0, name_order_section INTEGER NOT NULL DEFAULT 0,
  sort_name TEXT, album_artist_pid INTEGER NOT NULL DEFAULT 0, representative_item_pid INTEGER NOT NULL DEFAULT 0, feed_url TEXT);
CREATE TABLE genre (genre_id INTEGER PRIMARY KEY, genre TEXT NOT NULL, name_order INTEGER NOT NULL DEFAULT 0, name_order_section INTEGER NOT NULL DEFAULT 0);
CREATE TABLE composer (pid INTEGER PRIMARY KEY, name TEXT NOT NULL, name_order INTEGER NOT NULL DEFAULT 0);
CREATE TABLE base_location (base_location_id INTEGER PRIMARY KEY, path TEXT);
CREATE TABLE sort_map (name TEXT NOT NULL, name_order INTEGER NOT NULL, name_section INTEGER NOT NULL, sort_key BLOB);
CREATE TABLE container (pid INTEGER PRIMARY KEY, name TEXT);
CREATE TABLE container_item (pid INTEGER PRIMARY KEY, container_pid INTEGER, item_pid INTEGER, position INTEGER);
CREATE TABLE lyrics (item_pid INTEGER PRIMARY KEY, lyrics TEXT);
CREATE INDEX idx_item_album ON item (album_pid);
CREATE INDEX idx_artist_name ON artist (name COLLATE NOCASE);
"""

def connect(path):
    db = sqlite3.connect(path)
    db.create_collation("ML3_WEIRD", lambda a, b: (a.lower() > b.lower()) - (a.lower() < b.lower()))
    db.create_function("ml3_rev", 1, lambda x: 7)
    return db

def build_db(path, variant="full", unix=False, tracks=10):
    if os.path.exists(path): os.remove(path)
    db = sqlite3.connect(path)
    if variant == "minimal":
        db.executescript("""CREATE TABLE item (pid INTEGER PRIMARY KEY, title_order INTEGER, media_type INTEGER NOT NULL DEFAULT 8);
                            CREATE TABLE item_extra (item_pid INTEGER PRIMARY KEY, title TEXT, location TEXT NOT NULL DEFAULT '');""")
        db.execute("INSERT INTO item (pid,title_order) VALUES (1,5)")
        db.execute("INSERT INTO item_extra VALUES (1,'x','F01/AAAA.mp3')")
        db.commit(); db.close(); return
    db.executescript(SCHEMA_FULL)
    if variant == "collation":
        db.create_collation("ML3_WEIRD", lambda a, b: (a.lower() > b.lower()) - (a.lower() < b.lower()))
        db.executescript("CREATE INDEX idx_weird ON item_extra (title COLLATE ML3_WEIRD);")
    if variant == "trigger":
        db.executescript("""CREATE TRIGGER trg_item_ai AFTER INSERT ON item BEGIN
            UPDATE item SET revision_level = ml3_rev(NEW.pid) WHERE pid = NEW.pid; END;""")
        db.create_function("ml3_rev", 1, lambda x: 7)
    db.execute("INSERT INTO base_location VALUES (3840,'iTunes_Control/Music')")
    names = ["Adele", "Beck", "Coldplay", "Dido", "Eminem", "Foo Fighters", "Gorillaz", "Hole", "Interpol", "Jamiroquai"]
    base = 1_300_000_000 if unix else 400_000_000
    if variant != "empty":
        for i, n in enumerate(names[:tracks]):
            pid = 1000 + i
            db.execute("INSERT INTO sort_map VALUES (?,?,?,x'00')", (n, (i + 1) * 10, 1 + i))
            db.execute("INSERT INTO artist (pid,name,name_order,name_order_section,sort_name,representative_item_pid,keep_local) VALUES (?,?,?,?,?,?,1)",
                       (2000 + i, n, (i + 1) * 10, 1 + i, n.lower(), pid))
            db.execute("INSERT INTO album_artist (pid,name,name_order,name_order_section,sort_name,representative_item_pid) VALUES (?,?,?,?,?,?)",
                       (3000 + i, n, (i + 1) * 10, 1 + i, n.lower(), pid))
            db.execute("INSERT INTO album (pid,name,name_order,name_order_section,album_artist_pid,representative_item_pid,feed_url) VALUES (?,?,?,?,?,?,'http://x')",
                       (4000 + i, "Album " + n, (i + 1) * 10, 1 + i, 3000 + i, pid))
            db.execute("INSERT INTO genre VALUES (?,?,?,?)", (5000 + i, "Pop" if i == 0 else "G%d" % i, i, 1))
            db.execute("INSERT INTO item (pid,title_order,title_order_section,item_artist_pid,item_artist_order,album_pid,album_artist_pid,genre_id,base_location_id,date_added,date_downloaded,track_number,in_my_library) VALUES (?,?,?,?,?,?,?,?,3840,?,?,3,1)",
                       (pid, (i + 1) * 10, 1 + i, 2000 + i, (i + 1) * 10, 4000 + i, 3000 + i, 5000 + i, base + i, base + i))
            ext = "mp3" if i % 3 else "m4a"
            db.execute("INSERT INTO item_extra (item_pid,title,sort_title,total_time_ms,year,location,file_size,integrity,genius_id,date_modified) VALUES (?,?,?,?,?,?,?,x'DEADBEEF',99,?)",
                       (pid, "Song " + n, "song", 200000.0, 2000 + i, "F%02d/%s.%s" % (i, "ABC" + chr(65 + i), ext), 5000000, base))
            db.execute("INSERT INTO item_playback (item_pid,audio_format,bit_rate,data_kind,duration,sample_rate,volume_normalization_energy,format) VALUES (?,301,256,1,200000,44100.0,777,3)", (pid,))
            db.execute("INSERT INTO item_stats (item_pid,user_rating,play_count_user,has_been_played,date_played) VALUES (?,80,12,1,123456)", (pid,))
            db.execute("INSERT INTO item_store (item_pid,store_item_id,store_saga_id) VALUES (?,555,666)", (pid,))
            db.execute("INSERT INTO container_item (container_pid,item_pid,position) VALUES (1,?,?)", (pid, i))
            db.execute("INSERT INTO lyrics VALUES (?, 'la la')", (pid,))
    db.commit(); db.close()

def run(args, asan=True):
    p = subprocess.run([CLI] + args, capture_output=True, text=True, timeout=120)
    if p.returncode != 0 or "ERROR: AddressSanitizer" in p.stderr or "runtime error" in p.stderr:
        print(p.stdout[-3000:]); print(p.stderr[-3000:])
        check(False, "CLI zakończone błędem: %s" % args[:2])
    return p.stdout

def parse_results(out):
    ret = int(re.search(r"^RETURN (-?\d+)", out, re.M).group(1))
    res = []
    for m in re.finditer(r"^RESULT (\d+) (\d+) (\d+) (\S*) \|(.*?)\|(.*)$", out, re.M):
        ents = re.findall(r"ENT:(\w+):(\w+):(\w+):(\d+)", m.group(6))
        res.append(dict(i=int(m.group(1)), ok=int(m.group(2)), pid=int(m.group(3)), rel=m.group(4), err=m.group(5), ents=ents))
    return ret, res

def scenario(name, variant, unix=False, expect_ok=4, tracks=10):
    print("==", name)
    tmp = tempfile.mkdtemp(prefix="milib_")
    media = os.path.join(tmp, "Media")
    os.makedirs(os.path.join(media, "iTunes_Control", "iTunes"))
    os.makedirs(os.path.join(media, "iTunes_Control", "Music"))
    dbp = os.path.join(media, "iTunes_Control", "iTunes", "MediaLibrary.sqlitedb")
    build_db(dbp, variant, unix, tracks)
    bk = os.path.join(tmp, "Kopie")
    out = run(["import", dbp, bk, tmp])
    ret, res = parse_results(out)
    check(ret == expect_ok, "import zwrócił %d, oczekiwano %d\n%s" % (ret, expect_ok, out[-2500:]))
    check(len(os.listdir(bk)) == 1, "brak kopii bazy")
    db = connect(dbp)
    check(db.execute("PRAGMA integrity_check").fetchone()[0] == "ok", "integrity_check")
    nitems = db.execute("SELECT count(*) FROM item").fetchone()[0]
    check(nitems == (0 if variant == "empty" else (tracks if variant != "minimal" else 1)) + expect_ok, "liczba item=%d" % nitems)
    for r in res:
        if not r["ok"]: continue
        full = os.path.join(media, "iTunes_Control", "Music", r["rel"])
        check(os.path.isfile(full), "brak pliku " + full)
        check(re.match(r"^F\d\d/[A-Z]{4}\.(mp3|m4a|wav)$", r["rel"]) is not None, "zła nazwa pliku " + r["rel"])
        check(db.execute("SELECT count(*) FROM item WHERE pid=?", (r["pid"],)).fetchone()[0] == 1, "brak item")
        loc = db.execute("SELECT location FROM item_extra WHERE item_pid=?", (r["pid"],)).fetchone()
        check(loc is not None and loc[0] == r["rel"], "location w item_extra: %r" % (loc,))
    if variant == "full" or variant == "collation" or variant == "trigger" or variant == "unix":
        r0 = res[0]
        row = db.execute("SELECT item_artist_pid, album_pid, genre_id, track_number, disc_number, date_added, base_location_id, album_artist_pid, title_order FROM item WHERE pid=?", (r0["pid"],)).fetchone()
        check(row[0] != 0 and row[1] != 0 and row[2] != 0, "powiązania item: %r" % (row,))
        check(row[3] == 1, "track_number")
        want_date = 1_700_000_000 if unix else 1
        if unix: check(row[5] > 1_700_000_000, "data unix: %r" % row[5])
        else: check(0 < row[5] < 1_000_000_000, "data 2001: %r" % row[5])
        check(row[6] == 3840, "base_location")
        check(db.execute("SELECT name FROM artist WHERE pid=?", (row[0],)).fetchone() == ("Nowy Wykonawca",), "artist")
        check(db.execute("SELECT name FROM album WHERE pid=?", (row[1],)).fetchone() == ("Nowy Album",), "album")
        check(db.execute("SELECT genre FROM genre WHERE genre_id=?", (row[2],)).fetchone() == ("Rock",), "genre")
        check(db.execute("SELECT album_artist_pid FROM album WHERE pid=?", (row[1],)).fetchone()[0] == row[7], "album_artist_pid w album")
        # drugi utwór używa tych samych wierszy
        row2 = db.execute("SELECT item_artist_pid, album_pid, genre_id FROM item WHERE pid=?", (res[1]["pid"],)).fetchone()
        check(row2 == row[:3], "drugi utwór powinien współdzielić wykonawcę/album/gatunek: %r vs %r" % (row2, row[:3]))
        # liczba wierszy nowych wykonawców: Nowy Wykonawca + Istniejący (nowy, bo nie ma takiego)
        check(db.execute("SELECT count(*) FROM artist WHERE name='Nowy Wykonawca'").fetchone()[0] == 1, "duplikat wykonawcy")
        # nie skopiowano statystyk wzoru
        ex = db.execute("SELECT genius_id, integrity, total_time_ms, year FROM item_extra WHERE item_pid=?", (r0["pid"],)).fetchone()
        check(ex[0] == 0 and ex[1] is None, "wzór nie został wyczyszczony: %r" % (ex,))
        check(ex[2] == 215000 and ex[3] == 2021, "czas/rok: %r" % (ex,))
        st = db.execute("SELECT user_rating, play_count_user FROM item_stats WHERE item_pid=?", (r0["pid"],)).fetchone()
        check(st == (0, 0), "item_stats: %r" % (st,))
        so = db.execute("SELECT store_item_id FROM item_store WHERE item_pid=?", (r0["pid"],)).fetchone()
        check(so == (0,), "item_store: %r" % (so,))
        pb = db.execute("SELECT bit_rate, sample_rate, duration, volume_normalization_energy FROM item_playback WHERE item_pid=?", (r0["pid"],)).fetchone()
        check(pb == (192, 44100.0, 215000, 0), "item_playback: %r" % (pb,))
        # utwór bez tagów: tytuł zastępczy
        t3 = db.execute("SELECT title FROM item_extra WHERE item_pid=?", (res[3]["pid"],)).fetchone()
        check(t3 == ("Bez tytułu",), "tytuł zastępczy: %r" % (t3,))
    db.close()
    # usuwanie pierwszego utworu
    if res and res[0]["ok"]:
        args = ["remove", dbp, str(res[0]["pid"]), res[0]["rel"]]
        for e in res[0]["ents"]: args += list(e)
        out = run(args)
        check("RETURN 1" in out, "remove: %s" % out[-800:])
        db = connect(dbp)
        check(db.execute("SELECT count(*) FROM item WHERE pid=?", (res[0]["pid"],)).fetchone()[0] == 0, "item nie usunięty")
        check(db.execute("SELECT count(*) FROM item_extra WHERE item_pid=?", (res[0]["pid"],)).fetchone()[0] == 0, "item_extra nie usunięty")
        check(not os.path.exists(os.path.join(media, "iTunes_Control", "Music", res[0]["rel"])), "plik nie usunięty")
        if variant in ("full", "collation", "trigger", "unix"):
            check(db.execute("SELECT count(*) FROM artist WHERE name='Nowy Wykonawca'").fetchone()[0] == 1, "wykonawca użyty przez drugi utwór nie może zniknąć")
        # usuń drugi: wykonawca, album, gatunek powinni zniknąć
        if len(res) > 1 and res[1]["ok"] and variant in ("full", "collation", "trigger", "unix"):
            args = ["remove", dbp, str(res[1]["pid"]), res[1]["rel"]] + [x for e in res[0]["ents"] for x in e]
            out = run(args)
            check("RETURN 1" in out, "remove 2")
            db2 = connect(dbp)
            check(db2.execute("SELECT count(*) FROM artist WHERE name='Nowy Wykonawca'").fetchone()[0] == 0, "osierocony wykonawca nie został usunięty")
            check(db2.execute("SELECT count(*) FROM album WHERE name='Nowy Album'").fetchone()[0] == 0, "osierocony album nie został usunięty")
            db2.close()
        db.close()
    # diagnostyka
    out = run(["diagnose", dbp])
    check("RETURN 1" in out and "item_extra" in out, "diagnose")
    # przywracanie kopii
    out = run(["restore", dbp, bk])
    check("RETURN 1" in out, "restore: " + out[-300:])
    db = connect(dbp)
    nitems2 = db.execute("SELECT count(*) FROM item").fetchone()[0]
    check(nitems2 == nitems - expect_ok, "po przywróceniu kopii liczba item=%d, oczekiwano %d" % (nitems2, nitems - expect_ok))
    db.close()
    shutil.rmtree(tmp)
    return out

def main():
    subprocess.check_call(["gcc", "-g", "-O1", "-Wall", "-Wno-format-truncation", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined",
                           os.path.join(HERE, "lib_cli.c"), os.path.join(HERE, "..", "mi_lib.c"), os.path.join(HERE, "..", "mi_core.c"),
                           "-lsqlite3", "-o", CLI])
    scenario("pełny schemat, daty od 2001", "full")
    scenario("pełny schemat, daty unix", "unix", unix=True) if False else None
    scenario("daty unix", "full", unix=True)
    scenario("własne porównywanie napisów w indeksie", "collation", expect_ok=4)
    scenario("wyzwalacz z nieznaną funkcją", "trigger", expect_ok=4)
    scenario("pusta biblioteka", "empty")
    scenario("minimalny schemat", "minimal")
    # uszkodzona baza
    print("== uszkodzona baza")
    tmp = tempfile.mkdtemp(prefix="milib_")
    media = os.path.join(tmp, "Media")
    os.makedirs(os.path.join(media, "iTunes_Control", "iTunes")); os.makedirs(os.path.join(media, "iTunes_Control", "Music"))
    dbp = os.path.join(media, "iTunes_Control", "iTunes", "MediaLibrary.sqlitedb")
    open(dbp, "wb").write(b"to nie jest baza sqlite" * 100)
    out = run(["import", dbp, os.path.join(tmp, "Kopie"), tmp])
    ret, res = parse_results(out)
    check(ret == -1, "uszkodzona baza: ret=%d" % ret)
    check(not any(r["ok"] for r in res), "uszkodzona baza: nic nie powinno być zapisane")
    print("   ", out.strip().splitlines()[-1][:120])
    # baza zablokowana przez inny proces
    print("== baza zablokowana")
    tmp2 = tempfile.mkdtemp(prefix="milib_")
    media2 = os.path.join(tmp2, "Media")
    os.makedirs(os.path.join(media2, "iTunes_Control", "iTunes")); os.makedirs(os.path.join(media2, "iTunes_Control", "Music"))
    dbp2 = os.path.join(media2, "iTunes_Control", "iTunes", "MediaLibrary.sqlitedb")
    build_db(dbp2, "full")
    locker = sqlite3.connect(dbp2, isolation_level=None)
    locker.execute("BEGIN EXCLUSIVE")
    out = run(["import", dbp2, os.path.join(tmp2, "Kopie"), tmp2])
    ret, res = parse_results(out)
    check(ret == -1 and "zajęta" in out, "zablokowana baza: ret=%d\n%s" % (ret, out[-500:]))
    check(not any(os.listdir(os.path.join(media2, "iTunes_Control", "Music"))), "po nieudanym imporcie w folderze Muzyki nic nie powinno zostać")
    locker.execute("ROLLBACK"); locker.close()
    shutil.rmtree(tmp2)
    # brak bazy
    out = run(["import", os.path.join(tmp, "nie-ma.sqlitedb"), "-", tmp])
    ret, res = parse_results(out)
    check(ret == -1, "brak bazy")
    shutil.rmtree(tmp)
    print("BŁĘDÓW:", errors)
    sys.exit(1 if errors else 0)

main()
