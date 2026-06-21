import sqlite3
import csv
import os

def create_database():
    conn = sqlite3.connect('election.db')
    cursor = conn.cursor()

    # Create the secure table
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS voters(
            voter_id TEXT PRIMARY KEY,
            name TEXT,
            voted INTEGER DEFAULT 0,
            vote_time TEXT
        )
    ''')

    # Clear old data for a fresh start
    cursor.execute('DELETE FROM voters')

    csv_filename = 'voters.csv'

    # Check if the Excel/CSV file exists
    if not os.path.exists(csv_filename):
        print(f"ERROR: Could not find '{csv_filename}'. Make sure it is in the same folder!")
        return

    # Read the read-only CSV file
    with open(csv_filename, mode='r', encoding='utf-8') as file:
        reader = csv.reader(file)
        next(reader, None)  # Skip the header row (VoterID, Name)
        
        real_voters = []
        for row in reader:
            if len(row) >= 2: # Ensure the row has both ID and Name
                voter_id = row[0].strip()
                name = row[1].strip()
                # Append to our list: (ID, Name, Voted=0, Time=None)
                real_voters.append((voter_id, name, 0, None))

    # Insert everything into the secure database
    cursor.executemany('''
        INSERT INTO voters (voter_id, name, voted, vote_time)
        VALUES (?, ?, ?, ?)
    ''', real_voters)

    conn.commit()
    print(f"SUCCESS! Database updated with {len(real_voters)} voters from '{csv_filename}'.")
    conn.close()

if __name__ == "__main__":
    create_database()