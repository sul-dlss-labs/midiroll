//
// Programmer:    Craig Stuart Sapp <craig@ccrma.stanford.edu>
// Creation Date: Fri Apr 13 06:56:36 PDT 2018
// Last Modified: Fri Apr 13 08:11:17 PDT 2018
// Filename:      midiroll/src/MidiRoll.cpp
// Syntax:        C++11
// vim:           ts=3 expandtab
//
// Description:   A class for manipulating piano rolls in MIDI file format.
//


#include "MidiRoll.h"

#include <iostream>
#include <regex>

using namespace std;


//////////////////////////////
//
// MidiRoll::MidiRoll -- Class constructors.
//

MidiRoll::MidiRoll(void) : MidiFile() { }
MidiRoll::MidiRoll(const char* aFile) : MidiFile(aFile) { }
MidiRoll::MidiRoll(const string& aFile) : MidiFile(aFile) { }
MidiRoll::MidiRoll(istream& input) : MidiFile(input) { }
MidiRoll::MidiRoll(const MidiRoll& other) : MidiFile(other) { }
MidiRoll::MidiRoll(MidiRoll&& other) : MidiFile(other) { }



//////////////////////////////
//
// MidiRoll::MidiRoll -- Class deconstructor.
//

MidiRoll::~MidiRoll() { }



//////////////////////////////
//
// MidiRoll::operator= -- Copy constructor.
//

MidiRoll& MidiRoll::operator=(const MidiRoll& other) { 
	if (this == &other) {
		return *this;
	}
	MidiFile::operator=(other);
	return *this;
}

MidiRoll& MidiRoll::operator=(const MidiFile& other) { 
	MidiFile::operator=(other);
	return *this;
}



//////////////////////////////
//
// MidiRoll::setRollTempo -- Set the piano-roll tempo of the MIDI file.
//      The tempo is controlled by the ticks-per-quarter-note value in
//      the MIDI header rather than by a tempo meta message.  The tempo
//      meta messages are instead used to control an emulation of the
//      roll acceleration over time.  The final TPQ value will be rounded
//      to the nearest integer.
//
// default value: dpi = 300.0
// The default dpi of the rolls is set to 300 since this is the scanning
// resolution of the Stanford piano rolls (which are more precisely scanned
// at 300.25 dpi).  Each tick represents one image pixel row.
// A tempo of 100 means the roll moves (at its start) by
// 10.0 feet per minute.  This is 10.0 * 300 * 12 = 36000 rows/minute.
// A reference tempo of 60 bpm is used at the start of the acceleration,
// so each "quarter note" is 36000 / 60 = 600 rows.

void MidiRoll::setRollTempo(double tempo, double dpi) {
	int tpq = int(tempo/10.0 * dpi*12.0/60.0 + 0.5);
	if (tpq < 1) {
      // SMPTE ticks or error: do not alter
		cerr << "Error: tpq is too small: " << tpq << endl;
		return;
	}
	if (tpq > 0x7FFF) {
      // SMPTE ticks: do not alter
		cerr << "Error: tpq is too large: " << tpq << endl;
		return;
	}
	MidiFile::setTicksPerQuarterNote(tpq);
}



//////////////////////////////
//
// MidiRoll::getRollTempo -- Return the piano-roll tempo.
// default value: dpi = 300.0
//

double MidiRoll::getRollTempo(double dpi) {
	int tpq = MidiFile::getTicksPerQuarterNote();
	double tempo = tpq * 10.0 / dpi / 12.0 * 60.0;
	return tempo;
}



//////////////////////////////
//
// MidiRoll::getTextEvents -- Return a list of all MIDI events which are
//    meta-message text events.
//

vector<MidiEvent*> MidiRoll::getTextEvents(void) {
   vector<MidiEvent*> mes;
   for (int i=0; i<getTrackCount(); i++) {
      for (int j=0; j<operator[](i).getSize(); j++) {
         MidiEvent* mm = &operator[](i)[j];
         if (!mm->isMetaMessage()) {
            continue;
         }
         int mtype = mm->getMetaType();
         if (mtype != 0x01) {
            continue;
         }
         mes.push_back(mm);
      }
   }
   return mes;
}



//////////////////////////////
//
// MidiRoll::getMetadataEvents -- Return a list of all MIDI events
//    that are meta-message text events that have the structure of
//    a metadata key/value pair.
//

vector<MidiEvent*> MidiRoll::getMetadataEvents(void) {
   vector<MidiEvent*> mes;
   string marker = getMetadataMarker();
   for (int i=0; i<getTrackCount(); i++) {
      for (int j=0; j<operator[](i).getSize(); j++) {
         MidiEvent* mm = &operator[](i)[j];
         if (!mm->isMetaMessage()) {
            continue;
         }
         int mtype = mm->getMetaType();
         if (mtype != 0x01) {
            continue;
         }
         string content = mm->getMetaContent();
         if (content.compare(0, marker.size(), marker) != 0) {
            continue;
         }
         if (content.find(":", marker.size()) == string::npos) {
            continue;
         }
         mes.push_back(mm);
      }
   }
   return mes;
}



//////////////////////////////
//
// MidiRoll::metadata -- returns the value associated with a metadata key.
//     returns an empty string if the metadata key is not found.  The returned
//     value has whitespace trimmed from front and back of value.  Metadata will
//     only be searched for in the first track of the file.  Only the first
//     occurrence of the metadata key will be considered.
//

string MidiRoll::getMetadata(const string& key) {
   string output;
   string query;
   query += getMetadataMarker();
   query += key;
   query += ":\\s*(.*)\\s*$";
   regex re(query);
   smatch match;
   MidiRoll& mr = *this;
   for (int i=0; i<mr[0].size(); i++) {
      if (!mr[0][i].isText()) {
         continue;
      }
      string content = mr[0][i].getMetaContent();
      try {
         if (regex_search(content, match, re) && (match.size() > 1)) {
            output = match.str(1);
            break;
         }
      } catch (regex_error& e) {
         cerr << "PROBLEM SEARCHING FOR METADATA" << endl;
      }
   }
   return output;
}



//////////////////////////////
//
// MidiRoll::setMetadata -- Change the value of a given metadata key.
//    If there is no key for that metadata value, the add it.
//

int MidiRoll::setMetadata(const string& key, const string& value) {
   if (key.empty()) {
      cerr << "KEY CANNOT BE EMPTY" << endl;
      return -1;
   }
   bool found = false;
   int output = 0;
   string query;
   query += getMetadataMarker();
   query += key;
   query += ":\\s*(.*)\\s*$";
   regex re(query);
   smatch match;
   MidiRoll& mr = *this;
   for (int i=0; i<mr[0].size(); i++) {
      if (!mr[0][i].isText()) {
         continue;
      }
      string content = mr[0][i].getMetaContent();
      try {
         if (regex_search(content, match, re) && (match.size() > 1)) {
            string newline;
            newline += getMetadataMarker();
            newline += key;
            newline += ": ";
            newline += value;
            mr[0][i].setMetaContent(newline);
            found = true;
            output = mr[0][i].tick;
            break;
         }
      } catch (regex_error& e) {
         cerr << "PROBLEM SEARCHING FOR METADATA" << endl;
      }
   }
   if (found) {
      return output;
   }

   string newline;
   newline += getMetadataMarker();
   newline += key;
   newline += ": ";
   newline += value;

   mr.addText(0, 0, newline);
   mr.sortTrack(0);

   return output;
}



//////////////////////////////
//
// MidiRoll::trackerize --
//

void MidiRoll::trackerize(int trackerheight) {
   MidiRoll& mr = *this;
   
	mr.joinTracks();   // make a single list of events
	mr.linkNotePairs();
	
	for (int i=0; i<mr[0].getSize(); i++) {
		if (!mr[0][i].isNoteOn()) {
			continue;
		}
      MidiEvent* me = mr[0][i].getLinkedEvent();
      if (!me) {
         cerr << "MISSING NOTE OFF" << endl;
         continue;
      }
      me->tick += trackerheight;
	}

	mr.splitTracks(); // split events into separate tracks again
   mr.sortTracks();  // necessary since timestamps have been changed
}




//////////////////////////////
//
// MidiRoll::getLengthDpi -- Get the DPI resolution of the original scan
//    along the length of the piano roll.
//

double MidiRoll::getLengthDpi(void) {
   return m_lengthdpi;
}



//////////////////////////////
//
// MidiRoll::setLengthDpi -- Set the DPI resolution of the original scan
//     along the length of the piano roll.
//

void MidiRoll::setLengthDpi(double value) {
   if (value > 0) {
      m_lengthdpi = value;
   }
}



//////////////////////////////
//
// MidiRoll::getWidthDpi -- Get the DPI resolution of the original
//    scan across the width of the piano roll.
//

double MidiRoll::getWidthDpi(void) { 
   return m_widthdpi;
}



//////////////////////////////
//
// MidiRoll::setWidthDpi -- Set the DPI resolution of the original scan
//     across the width of the piano roll.
//

void MidiRoll::setWidthDpi(double value) {
   if (value > 0) {
      m_widthdpi = value;
   }
}



//////////////////////////////
//
// MidiRoll::getMetadataMarker --
//

string MidiRoll::getMetadataMarker(void) {
   return m_metadatamarker;
}



//////////////////////////////
//
// MidiRoll::getMetadataMarker --
//

void MidiRoll::setMetadataMarker(const string& value) {
   m_metadatamarker = value;
}



