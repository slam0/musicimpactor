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

def ddl(table, cols, pk):
    parts = []
    for name, typ in cols:
        if name == pk: parts.append("%s INTEGER PRIMARY KEY" % name)
        elif typ == "I": parts.append("%s INTEGER NOT NULL DEFAULT 0" % name)
        elif typ == "i": parts.append("%s INTEGER" % name)
        elif typ == "R": parts.append("%s REAL NOT NULL DEFAULT 0" % name)
        elif typ == "T": parts.append("%s TEXT" % name)
        elif typ == "B": parts.append("%s BLOB" % name)
        else: raise Exception(typ)
    return "CREATE TABLE %s (%s);" % (table, ", ".join(parts))

def cols(spec):
    out = []
    for w in spec.split():
        if ":" in w: n, t = w.split(":")
        else: n, t = w, "I"
        out.append((n, t))
    return out

# Układ wzięty z prawdziwego raportu z iPhone'a 4S (iOS 6.1.3)
ITEM = "item_pid media_type title_order title_order_section item_artist_pid item_artist_order item_artist_order_section series_name_order series_name_order_section album_pid album_order album_order_section album_artist_pid album_artist_order album_artist_order_section composer_pid composer_order composer_order_section genre_id genre_order genre_order_section disc_number track_number location_kind_id episode_sort_id store_link_id category_id base_location_id exclude_from_shuffle"
EXTRA = ("item_pid title:T sort_title:T disc_count track_count artwork_cache_id location_type location:T extension date_created file_size file_creator file_type "
         "num_dir_levels_file num_dir_levels_lib integrity:B media_kind date_modified year content_rating content_rating_level is_user_disabled remember_bookmark "
         "start_time_ms stop_time_ms total_time_ms:R total_burn_time_ms bpm relative_volume eq_preset:T genius_id comment:T grouping:T description:T description_long:T "
         "collection_description:T is_ota_purchased is_compilation is_itunes_u is_audible_audio_book copyright:T chosen_by_auto_fill is_temporary_cloud_download "
         "is_streaming_quality store_bookmark_timestamp store_bookmark_entity_revision is_podcast external_guid:T feed_url:T store_kind date_purchased date_released "
         "account_id key_versions key_platform_id key_id store_item_id store_artist_id store_composer_id store_genre_id store_playlist_id storefront_id "
         "redownload_action_params:B artwork_url:T store_xid:T store_flavor:T store_saga_id store_matched_status store_redownloaded_status audio_format sample_rate:R duration:R "
         "gapless_heuristic_info gapless_encoding_delay gapless_encoding_drain gapless_last_frame_resynch analysis_inhibit_flags audio_fingerprint "
         "volume_normalization_energy is_rental is_demo rental_duration rental_playback_duration rental_playback_date_started rental_date_started has_alternate_audio "
         "has_subtitles is_hd season_number audio_language:T audio_track_index audio_track_id subtitle_language:T subtitle_track_index subtitle_track_id episode_id:T")

def schema_real(weird_index=False):
    s = []
    s.append("CREATE TABLE _MLDatabaseProperties (key TEXT PRIMARY KEY, value);")
    s.append(ddl("album", cols("album_pid album:T sort_album:T album_artist_pid representative_item_pid grouping_key:B cloud_status artwork_cache_id user_rating all_compilations feed_url:T season_number"), "album_pid"))
    s.append(ddl("album_artist", cols("album_artist_pid album_artist:T sort_album_artist:T grouping_key:B cloud_status artwork_cache_id representative_item_pid"), "album_artist_pid"))
    s.append("CREATE TABLE artwork_info (cache_id INTEGER, format_id INTEGER, length INTEGER, offset INTEGER, extra_length INTEGER);")
    s.append(ddl("base_location", cols("base_location_id path:T"), "base_location_id"))
    s.append(ddl("chapter", cols("item_pid chapter_data:B"), "item_pid"))
    s.append(ddl("composer", cols("composer_pid composer:T sort_composer:T grouping_key:B cloud_status artwork_cache_id representative_item_pid"), "composer_pid"))
    s.append(ddl("container", cols("container_pid name:T"), "container_pid"))
    s.append("CREATE TABLE entity_revision (revision INTEGER PRIMARY KEY, entity_pid INTEGER, deleted INTEGER, class INTEGER, revision_type INTEGER);")
    s.append(ddl("genre", cols("genre_id genre:T grouping_key:B cloud_status representative_item_pid"), "genre_id"))
    s.append(ddl("item", cols(ITEM), "item_pid"))
    s.append(ddl("item_artist", cols("item_artist_pid item_artist:T sort_item_artist:T series_name:T sort_series_name:T grouping_key:B cloud_status representative_item_pid"), "item_artist_pid"))
    s.append(ddl("item_extra", cols(EXTRA), "item_pid"))
    s.append(ddl("item_search", cols("item_pid search_title search_album search_artist search_composer search_album_artist"), "item_pid"))
    s.append(ddl("item_stats", cols("item_pid has_been_played date_played play_count_user play_count_recent date_skipped skip_count_user skip_count_recent bookmark_time_ms user_rating hidden is_downloading download_identifier"), "item_pid"))
    s.append(ddl("location_kind", cols("location_kind_id kind:T"), "location_kind_id"))
    s.append(ddl("lyrics", cols("item_pid checksum lyrics:T"), "item_pid"))
    s.append(ddl("purgeable_list", cols("item_pid is_alarm date_last_used"), "item_pid"))
    s.append("CREATE TABLE sort_map (name TEXT NOT NULL, name_order INTEGER NOT NULL, name_section INTEGER NOT NULL, sort_key BLOB);")
    s.append(ddl("store_link", cols("store_link_id url:T"), "store_link_id"))
    s.append("CREATE TABLE container_items (container_pid INTEGER PRIMARY KEY, item_pid_data BLOB);")
    s.append("CREATE INDEX idx_item_album ON item (album_pid);")
    s.append("CREATE INDEX idx_artist_name ON item_artist (item_artist COLLATE NOCASE);")
    return "\n".join(s)

def connect(path):
    db = sqlite3.connect(path)
    db.create_collation("ML3_WEIRD", lambda a, b: (a.lower() > b.lower()) - (a.lower() < b.lower()))
    db.create_function("ml3_rev", 1, lambda x: 7)
    return db

STEP = 22171567396285518
SECTIONS = {"a": 0, "b": 2, "c": 3, "d": 5, "e": 6, "f": 8, "g": 9, "h": 10, "i": 11, "j": 12, "k": 13, "l": 14, "m": 16, "n": 17, "o": 19, "p": 20,
            "q": 21, "r": 22, "s": 23, "t": 25, "u": 26, "v": 27, "w": 28, "x": 29, "y": 30, "z": 31}

def gkey(name):
    out = bytearray()
    for ch in name.lower():
        if "a" <= ch <= "z": out.append(0x27 + 2 * (ord(ch) - 97))
        elif ch == " ": out.append(4)
        elif ch == "-": out.append(6)
        elif "0" <= ch <= "9": out.append(8 + int(ch))
        else: out.append(0x10 + (ord(ch) & 15))
    return bytes(out)

NAMES = ["Adele", "Beck", "Coldplay", "Dido", "Eminem", "Foo Fighters", "Gorillaz", "Hole", "Interpol", "Jamiroquai"]
FOLDERS = ["F%02d" % i for i in (0, 1, 2, 4, 6, 8, 10, 11)]
BASE_IDS = [-8771592297739899465, -7908286478552848395, -7907096613874481801, -6608787234879945858,
            -5958817943709028708, -4161266009262072806, -3295185654080969433, -1853857325077040168]

def build_db(path, variant="full", unix=False, tracks=10):
    if os.path.exists(path): os.remove(path)
    db = sqlite3.connect(path)
    if variant == "minimal":
        db.executescript("""CREATE TABLE item (item_pid INTEGER PRIMARY KEY, title_order INTEGER, media_type INTEGER NOT NULL DEFAULT 8);
                            CREATE TABLE item_extra (item_pid INTEGER PRIMARY KEY, title TEXT, location TEXT NOT NULL DEFAULT '');""")
        db.execute("INSERT INTO item (item_pid,title_order) VALUES (1,5)")
        db.execute("INSERT INTO item_extra VALUES (1,'x','AAAA.mp3')")
        db.commit(); db.close(); return
    db.executescript(schema_real())
    if variant == "collation":
        db.create_collation("ML3_WEIRD", lambda a, b: (a.lower() > b.lower()) - (a.lower() < b.lower()))
        db.executescript("CREATE INDEX idx_weird ON item_extra (title COLLATE ML3_WEIRD);")
    if variant == "trigger":
        db.create_function("ml3_rev", 1, lambda x: 7)
        db.executescript("""CREATE TRIGGER trg_item_ai AFTER INSERT ON item BEGIN
            UPDATE item SET episode_sort_id = ml3_rev(NEW.item_pid) WHERE item_pid = NEW.item_pid; END;""")
    for i, f in enumerate(FOLDERS):
        db.execute("INSERT INTO base_location VALUES (?,?)", (BASE_IDS[i], "iTunes_Control/Music/" + f))
    db.execute("INSERT INTO location_kind VALUES (1,'MPEG audio file')")
    db.execute("INSERT INTO composer (composer_pid,composer,sort_composer,grouping_key) VALUES (777,'','',x'')")
    # sort_map jak na prawdziwym iPhonie: wiersz dla każdej użytej nazwy, name_order rośnie z alfabetem
    allnames = set()
    for n in NAMES[:tracks]:
        allnames |= {"Song " + n, "Album " + n, n}
    allnames |= {"Pop"} | {"G%d" % i for i in range(1, tracks)}
    ordered = sorted(allnames, key=lambda x: x.lower())
    db.execute("INSERT INTO sort_map VALUES ('',0,0,x'')")
    for i, nm in enumerate(ordered):
        db.execute("INSERT INTO sort_map VALUES (?,?,?,?)", (nm, (i + 1) * STEP, SECTIONS[nm[0].lower()], gkey(nm)))
    db.execute("INSERT INTO sort_map VALUES ('300 dreams', 8214565720323784697, 34, x'00')")
    so = dict((r[0], (r[1], r[2])) for r in db.execute("SELECT name, name_order, name_section FROM sort_map"))
    base = 1_300_000_000 if unix else 400_000_000
    rev = 100
    if variant != "empty":
        for i, n in enumerate(NAMES[:tracks]):
            pid = 1000 + i
            sec = i + 1
            gname = "Pop" if i == 0 else "G%d" % i
            db.execute("INSERT INTO item_artist (item_artist_pid,item_artist,sort_item_artist,series_name,grouping_key,cloud_status,representative_item_pid) VALUES (?,?,?,'',?,5,?)",
                       (2000 + i, n, n, gkey(n), pid))
            db.execute("INSERT INTO album_artist (album_artist_pid,album_artist,sort_album_artist,grouping_key,artwork_cache_id,representative_item_pid) VALUES (?,?,?,?,77,?)",
                       (3000 + i, n, n, gkey(n), pid))
            db.execute("INSERT INTO album (album_pid,album,sort_album,album_artist_pid,representative_item_pid,grouping_key,artwork_cache_id,user_rating,feed_url) VALUES (?,?,?,?,?,?,77,60,'http://x')",
                       (4000 + i, "Album " + n, "Album " + n, 3000 + i, pid, gkey("Album " + n)))
            db.execute("INSERT INTO genre (genre_id,genre,grouping_key,representative_item_pid) VALUES (?,?,?,?)",
                       (5000 + i, gname, gkey(gname), pid))
            f = i % len(FOLDERS)
            db.execute("INSERT INTO item (item_pid,media_type,title_order,title_order_section,item_artist_pid,item_artist_order,item_artist_order_section,album_pid,album_order,album_order_section,"
                       "album_artist_pid,album_artist_order,album_artist_order_section,genre_id,genre_order,genre_order_section,track_number,location_kind_id,base_location_id) "
                       "VALUES (?,8,?,?,?,?,?,?,?,?,?,?,?,?,?,?,3,1,?)",
                       (pid, so["Song " + n][0], so["Song " + n][1], 2000 + i, so[n][0], so[n][1], 4000 + i, so["Album " + n][0], so["Album " + n][1],
                        3000 + i, so[n][0], so[n][1], 5000 + i, so[gname][0], so[gname][1], BASE_IDS[f]))
            ext = "mp3" if i % 3 else "m4a"
            fn = "ABC%s.%s" % (chr(65 + i), ext)
            db.execute("INSERT INTO item_extra (item_pid,title,sort_title,total_time_ms,year,location,file_size,integrity,genius_id,date_modified,date_created,extension,"
                       "location_type,audio_format,sample_rate,duration,artwork_cache_id,comment,store_item_id,file_type,store_kind,store_flavor,gapless_encoding_delay) "
                       "VALUES (?,?,?,?,?,?,?,x'DEADBEEF',99,?,?,0,0,?,44100.0,8820000,77,'komentarz',555,0,1,'2:256',528)",
                       (pid, "Song " + n, "Song " + n, 200000.0, 2000 + i, fn, 5000000, base, base,
                        301 if ext == "mp3" else 502))
            db.execute("INSERT INTO item_search VALUES (?,?,?,?,0,?)", (pid, so["Song " + n][0], so["Album " + n][0], so[n][0], so[n][0]))
            db.execute("INSERT INTO item_stats (item_pid,user_rating,play_count_user,has_been_played,date_played,download_identifier) VALUES (?,80,12,1,123456,9)", (pid,))
            db.execute("INSERT INTO purgeable_list VALUES (?,0,?)", (pid, base))
            for cls, key in ((0, pid), (1, 2000 + i), (2, 4000 + i), (3, 3000 + i), (4, 5000 + i)):
                rev += 1
                db.execute("INSERT INTO entity_revision VALUES (?,?,0,?,0)", (rev, key, cls))
    db.execute("INSERT INTO _MLDatabaseProperties VALUES ('LibraryRevision', ?)", (rev,))
    db.execute("INSERT INTO _MLDatabaseProperties VALUES ('DatabaseVersion', 1111)")
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

PER_ITEM = ["item_extra", "item_search", "item_stats", "purgeable_list"]

def scenario(name, variant, unix=False, expect_ok=4, tracks=10):
    print("==", name)
    tmp = tempfile.mkdtemp(prefix="milib_")
    media = os.path.join(tmp, "Media")
    os.makedirs(os.path.join(media, "iTunes_Control", "iTunes"))
    os.makedirs(os.path.join(media, "iTunes_Control", "Music"))
    dbp = os.path.join(media, "iTunes_Control", "iTunes", "MediaLibrary.sqlitedb")
    build_db(dbp, variant, unix, tracks)
    bk = os.path.join(tmp, "Kopie")
    fulltype = variant in ("full", "collation", "trigger")
    if fulltype:
        d0 = connect(dbp)
        old_rev = d0.execute("SELECT max(revision) FROM entity_revision").fetchone()[0]
        d0.close()
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
        check(db.execute("SELECT count(*) FROM item WHERE item_pid=?", (r["pid"],)).fetchone()[0] == 1, "brak item")
        loc = db.execute("SELECT location FROM item_extra WHERE item_pid=?", (r["pid"],)).fetchone()
        if variant == "minimal":
            check(loc is not None and loc[0] == r["rel"], "location w item_extra: %r" % (loc,))
        else:
            folder, fname = r["rel"].split("/")
            check(loc is not None and loc[0] == fname, "location powinno być samą nazwą pliku: %r" % (loc,))
            bl = db.execute("SELECT path FROM base_location WHERE base_location_id = (SELECT base_location_id FROM item WHERE item_pid=?)", (r["pid"],)).fetchone()
            check(bl is not None and bl[0] == "iTunes_Control/Music/" + folder, "base_location nie zgadza się z folderem pliku: %r vs %s" % (bl, folder))
    if variant in ("full", "collation", "trigger", "empty"):
        for r in res:
            if not r["ok"]: continue
            for t in PER_ITEM:
                if variant == "empty" and t == "purgeable_list": continue
                check(db.execute("SELECT count(*) FROM %s WHERE item_pid=?" % t, (r["pid"],)).fetchone()[0] == 1, "brak wiersza w %s" % t)
    if fulltype:
        r0 = res[0]
        row = db.execute("SELECT item_artist_pid, album_pid, genre_id, track_number, disc_number, base_location_id, album_artist_pid, title_order, title_order_section, media_type, location_kind_id FROM item WHERE item_pid=?", (r0["pid"],)).fetchone()
        check(row[0] != 0 and row[1] != 0 and row[2] != 0, "powiązania item: %r" % (row,))
        check(row[3] == 1, "track_number")
        check(row[9] == 8 and row[10] == 1, "media_type/location_kind_id ze wzoru: %r" % (row,))
        sm = dict((r[0], (r[1], r[2])) for r in db.execute("SELECT name, name_order, name_section FROM sort_map"))
        tname = "Zażółć gęślą jaźń"
        check(tname in sm and sm[tname] == (row[7], row[8]), "sort_map nie ma nowego tytułu albo różni się od item: %r vs %r" % (sm.get(tname), row[7:9]))
        check(row[8] == 31, "sekcja litery Z: %r" % (row[8],))
        letters = [v[0] for k, v in sm.items() if v[1] < 34 and k != tname]
        check(row[7] > max(letters), "Z... powinno być po wszystkich istniejących nazwach")
        nw = sm.get("Nowy Wykonawca")
        check(nw is not None and nw[1] == 17 and sm["Jamiroquai"][0] < nw[0] < sm["Song Adele"][0], "kolejność wykonawcy w sort_map: %r" % (nw,))
        check(sm["Jamiroquai"][0] < sm["Song Adele"][0], "sort_map sprawdzian")
        icomp = db.execute("SELECT composer_pid, composer_order, composer_order_section FROM item WHERE item_pid=?", (r0["pid"],)).fetchone()
        check(icomp == (0, 0, 0), "brak kompozytora: composer_pid/order = 0: %r" % (icomp,))
        check(db.execute("SELECT item_artist FROM item_artist WHERE item_artist_pid=?", (row[0],)).fetchone() == ("Nowy Wykonawca",), "item_artist")
        check(db.execute("SELECT album FROM album WHERE album_pid=?", (row[1],)).fetchone() == ("Nowy Album",), "album")
        check(db.execute("SELECT genre FROM genre WHERE genre_id=?", (row[2],)).fetchone() == ("Rock",), "genre")
        check(db.execute("SELECT album_artist FROM album_artist WHERE album_artist_pid=?", (row[6],)).fetchone() == ("Nowy Wykonawca",), "album_artist")
        check(db.execute("SELECT album_artist_pid FROM album WHERE album_pid=?", (row[1],)).fetchone()[0] == row[6], "album_artist_pid w album")
        # wiersze pomocnicze nie dziedziczą cudzych danych ze wzoru
        a = db.execute("SELECT grouping_key, cloud_status, representative_item_pid, sort_item_artist FROM item_artist WHERE item_artist_pid=?", (row[0],)).fetchone()
        check(a[0] == gkey("Nowy Wykonawca") and a[1] == 0 and a[2] == r0["pid"] and a[3] == "Nowy Wykonawca", "item_artist: %r" % (a,))
        al = db.execute("SELECT artwork_cache_id, user_rating, feed_url, grouping_key FROM album WHERE album_pid=?", (row[1],)).fetchone()
        check(al[0] == 0 and al[1] == 0 and al[2] is None and al[3] == gkey("Nowy Album"), "album: %r" % (al,))
        check(db.execute("SELECT artwork_cache_id FROM album_artist WHERE album_artist_pid=?", (row[6],)).fetchone() == (0,), "album_artist artwork")
        # drugi utwór używa tych samych wierszy
        row2 = db.execute("SELECT item_artist_pid, album_pid, genre_id FROM item WHERE item_pid=?", (res[1]["pid"],)).fetchone()
        check(row2 == row[:3], "drugi utwór powinien współdzielić wykonawcę/album/gatunek: %r vs %r" % (row2, row[:3]))
        check(db.execute("SELECT count(*) FROM item_artist WHERE item_artist='Nowy Wykonawca'").fetchone()[0] == 1, "duplikat wykonawcy")
        # item_extra: pola utworu i czyszczenie wzoru
        ex = db.execute("SELECT genius_id, integrity, total_time_ms, year, duration, sample_rate, extension, audio_format, artwork_cache_id, comment, store_item_id, "
                        "store_kind, store_flavor, file_size, date_created, date_modified, title, sort_title FROM item_extra WHERE item_pid=?", (r0["pid"],)).fetchone()
        check(ex[0] == 0 and ex[1] is None, "wzór nie został wyczyszczony: %r" % (ex,))
        check(ex[2] == 215000 and ex[3] == 2021, "czas/rok: %r" % (ex,))
        check(ex[4] == 215 * 44100, "duration w próbkach: %r" % (ex[4],))
        check(ex[5] == 44100, "sample_rate")
        check(ex[7] == 301, "kod formatu mp3 ze wzoru: %r" % (ex[6:8],))
        check(ex[8] == 0 and ex[9] is None and ex[10] == 0, "okładka/komentarz/sklep: %r" % (ex[8:11],))
        check(ex[11] == 1 and ex[12] == "2:256", "store_kind/store_flavor zostają ze wzoru: %r" % (ex[11:13],))
        check(ex[13] == 4010, "file_size: %r" % (ex[13],))
        if unix: check(ex[14] > 1_700_000_000 and ex[15] > 1_700_000_000, "data unix: %r" % (ex[14:16],))
        else: check(0 < ex[14] < 1_000_000_000 and 0 < ex[15] < 1_000_000_000, "data 2001: %r" % (ex[14:16],))
        check(ex[16] == "Zażółć gęślą jaźń" and ex[17] == ex[16], "title/sort_title")
        ex2 = db.execute("SELECT extension, audio_format, duration FROM item_extra WHERE item_pid=?", (res[2]["pid"],)).fetchone()
        check(ex2[1] == 502 and ex2[2] == 60 * 44100, "kody formatu m4a ze wzoru: %r" % (ex2,))
        srch = db.execute("SELECT search_title, search_album, search_artist, search_composer, search_album_artist FROM item_search WHERE item_pid=?", (r0["pid"],)).fetchone()
        ords = db.execute("SELECT title_order, album_order, item_artist_order, composer_order, album_artist_order FROM item WHERE item_pid=?", (r0["pid"],)).fetchone()
        check(srch == ords and srch[0] != 0, "item_search powinno powtarzać liczby sortowania: %r vs %r" % (srch, ords))
        st = db.execute("SELECT user_rating, play_count_user, has_been_played, date_played, download_identifier FROM item_stats WHERE item_pid=?", (r0["pid"],)).fetchone()
        check(st == (0, 0, 0, 0, 0), "item_stats: %r" % (st,))
        pl = db.execute("SELECT is_alarm, date_last_used FROM purgeable_list WHERE item_pid=?", (r0["pid"],)).fetchone()
        check(pl[0] == 0 and pl[1] > 0, "purgeable_list: %r" % (pl,))
        # utwór bez tagów: tytuł zastępczy
        t3 = db.execute("SELECT title FROM item_extra WHERE item_pid=?", (res[3]["pid"],)).fetchone()
        check(t3 == ("Bez tytułu",), "tytuł zastępczy: %r" % (t3,))
        # dziennik zmian
        rv = dict(((a, b), c) for a, b, c in db.execute("SELECT entity_pid, class, revision FROM entity_revision WHERE revision > ?", (old_rev,)))
        check((r0["pid"], 0) in rv, "entity_revision dla utworu")
        check((row[0], 1) in rv and (row[1], 2) in rv and (row[6], 3) in rv and (row[2], 4) in rv, "entity_revision dla wykonawcy/albumu/gatunku: %r" % (rv,))
        newmax = db.execute("SELECT max(revision) FROM entity_revision").fetchone()[0]
        check(sorted(rv.values()) == list(range(old_rev + 1, newmax + 1)), "numery wersji powinny być ciągłe")
        check(db.execute("SELECT value FROM _MLDatabaseProperties WHERE key='LibraryRevision'").fetchone()[0] == newmax, "licznik wersji")
        check(db.execute("SELECT value FROM _MLDatabaseProperties WHERE key='DatabaseVersion'").fetchone()[0] == 1111, "inne właściwości bez zmian")
    db.close()
    # usuwanie pierwszego utworu
    if res and res[0]["ok"]:
        args = ["remove", dbp, str(res[0]["pid"]), res[0]["rel"]]
        for e in res[0]["ents"]: args += list(e)
        out = run(args)
        check("RETURN 1" in out, "remove: %s" % out[-800:])
        db = connect(dbp)
        check(db.execute("SELECT count(*) FROM item WHERE item_pid=?", (res[0]["pid"],)).fetchone()[0] == 0, "item nie usunięty")
        for t in (PER_ITEM if variant != "minimal" else ["item_extra"]):
            check(db.execute("SELECT count(*) FROM %s WHERE item_pid=?" % t, (res[0]["pid"],)).fetchone()[0] == 0, "%s nie usunięty" % t)
        check(not os.path.exists(os.path.join(media, "iTunes_Control", "Music", res[0]["rel"])), "plik nie usunięty")
        if fulltype:
            check(db.execute("SELECT count(*) FROM entity_revision WHERE entity_pid=?", (res[0]["pid"],)).fetchone()[0] == 0, "entity_revision utworu nie usunięty")
            check(db.execute("SELECT count(*) FROM item_artist WHERE item_artist='Nowy Wykonawca'").fetchone()[0] == 1, "wykonawca użyty przez drugi utwór nie może zniknąć")
            rp = db.execute("SELECT representative_item_pid FROM item_artist WHERE item_artist='Nowy Wykonawca'").fetchone()[0]
            check(rp == res[1]["pid"], "representative_item_pid powinien wskazywać na drugi utwór: %r" % rp)
            rp = db.execute("SELECT representative_item_pid FROM album WHERE album='Nowy Album'").fetchone()[0]
            check(rp == res[1]["pid"], "representative_item_pid albumu")
        # usuń drugi: wykonawca, album, gatunek powinni zniknąć
        if len(res) > 1 and res[1]["ok"] and fulltype:
            args = ["remove", dbp, str(res[1]["pid"]), res[1]["rel"]] + [x for e in res[0]["ents"] for x in e]
            out = run(args)
            check("RETURN 1" in out, "remove 2")
            db2 = connect(dbp)
            check(db2.execute("SELECT count(*) FROM item_artist WHERE item_artist='Nowy Wykonawca'").fetchone()[0] == 0, "osierocony wykonawca nie został usunięty")
            check(db2.execute("SELECT count(*) FROM album WHERE album='Nowy Album'").fetchone()[0] == 0, "osierocony album nie został usunięty")
            check(db2.execute("SELECT count(*) FROM genre WHERE genre='Rock'").fetchone()[0] == 0, "osierocony gatunek nie został usunięty")
            for ent in res[0]["ents"]:
                check(db2.execute("SELECT count(*) FROM entity_revision WHERE entity_pid=?", (int(ent[3]),)).fetchone()[0] == 0, "entity_revision encji %s nie usunięty" % ent[0])
            db2.close()
        db.close()
    # diagnostyka
    out = run(["diagnose", dbp])
    check("RETURN 1" in out and "item_extra" in out, "diagnose")
    if fulltype:
        check("Przykładowy utwór item_pid=" in out and "base_location" in out and "_MLDatabaseProperties" in out, "diagnose: brak próbek")
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
    print("== sortowanie, sekcje, klucze (dane z prawdziwego iPhone'a)")
    subprocess.check_call(["gcc", "-O1", "-Wno-format-truncation", "-I" + os.path.join(HERE, ".."), os.path.join(HERE, "sort_test.c"),
                           os.path.join(HERE, "..", "mi_core.c"), "-lsqlite3", "-o", os.path.join(HERE, "sort_test")])
    p = subprocess.run([os.path.join(HERE, "sort_test")], capture_output=True, text=True)
    check(p.returncode == 0 and "bad=0" in p.stdout, "sort_test: " + p.stdout)
    scenario("pełny schemat, daty od 2001", "full")
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
