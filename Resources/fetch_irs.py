"""
One-time fetch script: downloads the curated starter IR set from itsmusician/IR-Library
(MIT License) into Resources/IRs/<Category>/. Not needed at plugin runtime -- this is a
build-time/dev asset step. Re-run if Resources/IRs is ever cleaned.
"""
import os
import urllib.request
import urllib.parse

RAW_BASE = "https://raw.githubusercontent.com/itsmusician/IR-Library/main/"
DEST_ROOT = os.path.join(os.path.dirname(__file__), "IRs")

# (source path in the GitHub repo, destination relative path under Resources/IRs)
FILES = [
    ("Rooms/Residential/Arroyo House/Arroyo House Living Room Close A.wav", "Residential/Arroyo House Living Room.wav"),
    ("Rooms/Residential/College House/College House Bedroom Stereo 1.wav", "Residential/College House Bedroom.wav"),
    ("Rooms/Residential/College House/College House Master Bathroom.wav", "Residential/College House Master Bathroom.wav"),
    ("Rooms/Residential/Old Home/Old Home Living Room.wav", "Residential/Old Home Living Room.wav"),
    ("Rooms/Residential/Ranch House/Ranch House Bathroom 1 A.wav", "Residential/Ranch House Bathroom.wav"),
    ("Rooms/Residential/Colonial House/Colonial Bedroom (No Treatment).wav", "Residential/Colonial House Bedroom.wav"),

    ("Rooms/Commercial/Hotels/Ballrooms/Royal Ballroom.wav", "Commercial/Royal Ballroom.wav"),
    ("Rooms/Commercial/Hotels/Bathrooms/Lux Hotel Bathroom.wav", "Commercial/Lux Hotel Bathroom.wav"),
    ("Rooms/Commercial/Parking Garages/Firenze Parking Garage.wav", "Commercial/Firenze Parking Garage.wav"),
    ("Rooms/Commercial/Shops/Craft Coffee Shop/Craft Coffee Shop Afar.wav", "Commercial/Craft Coffee Shop.wav"),
    ("Rooms/Commercial/Shops/Tall Coffee Shop/Tall Coffee Shop Lobby.wav", "Commercial/Tall Coffee Shop Lobby.wav"),

    ("Rooms/Public/Arenas/Giant Center/Giant Center Temporary Flown PA Upper Bleachers (Low Gain).wav", "Public/Giant Center Arena.wav"),
    ("Rooms/Public/Colleges & Universities/University of Central Florida/Engineering II Atrium Stereo Clap.wav", "Public/UCF Engineering Atrium.wav"),
    ("Rooms/Public/Colleges & Universities/University of Central Florida/Old Audio Engineering Club Room.wav", "Public/UCF Audio Club Room.wav"),
    ("Rooms/Public/Convention Centers/Phoenix Convention Center/PCC North 300 Level Exhibit Hall XY Balloon Far.wav", "Public/Phoenix Convention Center Hall.wav"),

    ("Rooms/Historical/Landmarks/The Pantheon (Rome)/The Pantheon Optimal.wav", "Historical/The Pantheon Rome.wav"),
    ("Rooms/Historical/Religious/Drumheller's Little Church/Drumheller's Little Church.wav", "Historical/Drumhellers Little Church.wav"),
    ("Rooms/Historical/Religious/Sunnybrook Farm Willowdale Church/Sunnybrook Farm Willowdale Church.wav", "Historical/Sunnybrook Willowdale Church.wav"),
    ("Rooms/Historical/Shelters/Menorcan Spanish Civil War Bunkers/Bunker 1 Entry Near.wav", "Historical/Menorcan Civil War Bunker.wav"),

    ("Springs/HG Spring/HG Spring Stereo Loud.wav", "Textures/HG Spring.wav"),
    ("Springs/Letter Holding Coil/Letter Holding Coil Stereo.wav", "Textures/Letter Holding Coil Spring.wav"),
    ("Plates/Conner Plate I/Conner Plate I Impacts/Conner Plate I Drum Stick Kit Medium.wav", "Textures/Conner Plate Impact.wav"),

    # --- Second batch (appended, keeps existing indices stable for saved presets/automation) ---
    ("Rooms/Residential/13th Century Venetian Home/13th Century Venetian Home.wav", "Residential/13th Century Venetian Home.wav"),
    ("Rooms/Residential/Villa 10/Villa 10 Hall Distant.wav", "Residential/Villa 10 Hall.wav"),
    ("Rooms/Residential/Old Home/Old Home Fireplace.wav", "Residential/Old Home Fireplace.wav"),
    ("Rooms/Residential/Ranch House/Ranch House Hall 1 A Open.wav", "Residential/Ranch House Hall.wav"),

    ("Rooms/Commercial/Hotels/Ballrooms/Palm Ballroom.wav", "Commercial/Palm Ballroom.wav"),
    ("Rooms/Commercial/Hotels/Bathrooms/Reflective Half Bathroom.wav", "Commercial/Reflective Half Bathroom.wav"),
    ("Rooms/Commercial/Hotels/Guest Rooms/Hotel Room 1 XY (Balloon).wav", "Commercial/Hotel Guest Room.wav"),
    ("Rooms/Commercial/Shops/Ancient Wand Shop/Ancient Wand Shop.wav", "Commercial/Ancient Wand Shop.wav"),

    ("Rooms/Public/Colleges & Universities/Missouri State University/MSU Arena Court Seating (Heavy Denoise).wav", "Public/MSU Arena Court Seating.wav"),
    ("Rooms/Public/Convention Centers/Fort Worth Convention Center/FWCC Arena Balloon.wav", "Public/Fort Worth Convention Arena.wav"),
    ("Rooms/Public/Convention Centers/Orange County Convention Center/OCCC Freight Elevator.wav", "Public/OCCC Freight Elevator.wav"),
    ("Rooms/Public/Miscellaneous Public/Weston's Backrooms Hall Mono Snap.wav", "Public/Westons Backrooms Hall.wav"),

    ("Rooms/Historical/Shelters/Menorcan Spanish Civil War Bunkers/Bunker 2 Stair Top.wav", "Historical/Menorcan Bunker Stairwell.wav"),

    ("Outdoors/Historical/Ostia Antica Theatre/Ostia Antica Theatre.wav", "Outdoors/Ostia Antica Theatre.wav"),
    ("Outdoors/Public/Forests/Small Forest Clearing 1 Balloon.wav", "Outdoors/Forest Clearing.wav"),
    ("Outdoors/Public/Parks/Rockefeller Gardens Field Stage.wav", "Outdoors/Rockefeller Gardens Field Stage.wav"),
    ("Outdoors/Public/Parks/Florida SR40 Bridge Underpass.wav", "Outdoors/Bridge Underpass.wav"),
    ("Outdoors/Public/Streets/Suburb Street Distant Bang Snap.wav", "Outdoors/Suburb Street.wav"),
    ("Outdoors/Public/Colleges & Universities/University Garage 1 Stairwell.wav", "Outdoors/University Garage Stairwell.wav"),
    ("Outdoors/Commercial/Loading Docks/Eerie Loading Dock.wav", "Outdoors/Eerie Loading Dock.wav"),
    ("Outdoors/Residential/Apartments/Rome Apartment Alley/Rome Apartment Alley.wav", "Outdoors/Rome Apartment Alley.wav"),

    ("Enclosed Spaces/Automobiles/2002 Sedan/2002 Sedan Speakers Front.wav", "Textures/Car Interior.wav"),
    ("Enclosed Spaces/Miscellaneous/Storm Drain Bang Snap.wav", "Textures/Storm Drain.wav"),
    ("Enclosed Spaces/Watercraft/Catamaran Hull.wav", "Textures/Catamaran Hull.wav"),
]

def main():
    ok, failed, skipped = 0, [], 0
    for src, dest in FILES:
        url = RAW_BASE + urllib.parse.quote(src)
        destPath = os.path.join(DEST_ROOT, dest)
        os.makedirs(os.path.dirname(destPath), exist_ok=True)
        if os.path.exists(destPath):
            skipped += 1
            continue
        try:
            urllib.request.urlretrieve(url, destPath)
            size = os.path.getsize(destPath)
            print(f"OK  {size/1024/1024:6.2f} MB  {dest}")
            ok += 1
        except Exception as e:
            print(f"FAIL {dest}: {e}")
            failed.append(dest)

    print(f"\n{ok}/{len(FILES)} downloaded, {skipped} already present.")
    if failed:
        print("Failed:", failed)

if __name__ == "__main__":
    main()
