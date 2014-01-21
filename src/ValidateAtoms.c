/*

This file contains Original Code and/or Modifications of Original Code
as defined in and that are subject to the Apple Public Source License
Version 2.0 (the 'License'). You may not use this file except in
compliance with the License. Please obtain a copy of the License at
http://www.opensource.apple.com/apsl/ and read it before using this
file.

The Original Code and all software distributed under the License are
distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
Please see the License for the specific language governing rights and
limitations under the License.

*/

#include "ValidateMP4.h"


extern ValidateGlobals vg;

//===============================================

OSErr CheckMatrixForUnity( MatrixRecord mr )
{
	OSErr err = noErr;
	if ( 	(mr[0][0] != 0x00010000)
		||  (mr[0][1] != 0)
		||  (mr[0][2] != 0)
		||  (mr[1][0] != 0)
		||  (mr[1][1] != 0x00010000)
		||  (mr[1][2] != 0)
		||  (mr[2][0] != 0)
		||  (mr[2][1] != 0)
		||  (mr[2][2] != 0x40000000)
		) {
		err = badAtomErr;
		errprint("has non-identity matrix" "\n"); 
	}
	return err;
}

//===============================================

OSErr GetFullAtomVersionFlags( atomOffsetEntry *aoe, UInt32 *version, UInt32 *flags, UInt64 *offsetOut)
{
	OSErr err = noErr;
	UInt32 versFlags;
	UInt64 offset;
	
	// Get version/flags
	offset = aoe->offset + aoe->atomStartSize;
	BAILIFERR( GetFileDataN32( aoe, &versFlags, offset, &offset ) );
	*version = (versFlags >> 24) & 0xFF;
	*flags   =  versFlags & 0x00ffffff;
	*offsetOut = offset;
	
bail:
	return err;
}

//==========================================================================================

OSErr Validate_iods_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;
	Ptr odDataP = nil;
	unsigned long odSize;
	
	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );
	FieldMustBe( flags, 0, "'iods' version must be %d not %d" );
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);
	atomprint("/>\n"); 
	
	// Get the ObjectDescriptor
	BAILIFERR( GetFileBitStreamDataToEndOfAtom( aoe, &odDataP, &odSize, offset, &offset ) );
	BAILIFERR( Validate_iods_OD_Bits( odDataP, odSize, true ) );
		
	// All done
	aoe->aoeflags |= kAtomValidated;
	
bail:
	if (odDataP)
		free(odDataP);

	return err;
}

//==========================================================================================

typedef struct MovieHeaderCommonRecord {
    Fixed                           preferredRate;              // must be 1.0 for mp4

    SInt16                          preferredVolume;           	// must be 1.0 for mp4
    short                           reserved1;					// must be 0

    long                            preferredLong1;				// must be 0 for mp4
    long                            preferredLong2;				// must be 0 for mp4

    MatrixRecord                    matrix;						// must be identity for mp4

    TimeValue                       previewTime;				// must be 0 for mp4
    TimeValue                       previewDuration;			// must be 0 for mp4

    TimeValue                       posterTime;    				// must be 0 for mp4

    TimeValue                       selectionTime;  			// must be 0 for mp4
    TimeValue                       selectionDuration;  		// must be 0 for mp4
    TimeValue                       currentTime;          		// must be 0 for mp4

    long                            nextTrackID;
} MovieHeaderCommonRecord;

typedef struct MovieHeaderVers0Record {
    UInt32    				creationTime;
    UInt32    				modificationTime;
    UInt32            		timeScale;
    UInt32	            	duration;
} MovieHeaderVers0Record;

typedef struct MovieHeaderVers1Record {
    UInt64    				creationTime;
    UInt64    				modificationTime;
    UInt32            		timeScale;
    UInt64					duration;
} MovieHeaderVers1Record;
	
OSErr Validate_mvhd_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;
	MovieHeaderVers1Record	mvhdHead;
	MovieHeaderCommonRecord	mvhdHeadCommon;
	
	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );
	

	// Get data based on version
	if (version == 0) {
		MovieHeaderVers0Record	mvhdHead0;
		BAILIFERR( GetFileData( aoe, &mvhdHead0, offset, sizeof(mvhdHead0), &offset ) );
		mvhdHead.creationTime = EndianU32_BtoN(mvhdHead0.creationTime);
		mvhdHead.modificationTime = EndianU32_BtoN(mvhdHead0.modificationTime);
		mvhdHead.timeScale = EndianU32_BtoN(mvhdHead0.timeScale);
		mvhdHead.duration = EndianU32_BtoN(mvhdHead0.duration);
	} else if (version == 1) {
		BAILIFERR( GetFileData( aoe, &mvhdHead.creationTime, offset, sizeof(mvhdHead.creationTime), &offset ) );
		mvhdHead.creationTime = EndianU64_BtoN(mvhdHead.creationTime);
		BAILIFERR( GetFileData( aoe, &mvhdHead.modificationTime, offset, sizeof(mvhdHead.modificationTime), &offset ) );
		mvhdHead.modificationTime = EndianU64_BtoN(mvhdHead.modificationTime);
		BAILIFERR( GetFileData( aoe, &mvhdHead.timeScale, offset, sizeof(mvhdHead.timeScale), &offset ) );
		mvhdHead.timeScale = EndianU32_BtoN(mvhdHead.timeScale);
		BAILIFERR( GetFileData( aoe, &mvhdHead.duration, offset, sizeof(mvhdHead.duration), &offset ) );
		mvhdHead.duration = EndianU64_BtoN(mvhdHead.duration);
	} else {
		errprint("Movie header is version other than 0 or 1\n");
		err = badAtomErr;
		goto bail;
	}
	
	BAILIFERR( GetFileData( aoe, &mvhdHeadCommon, offset, sizeof(mvhdHeadCommon), &offset ) );
    mvhdHeadCommon.preferredRate = EndianU32_BtoN(mvhdHeadCommon.preferredRate);
    mvhdHeadCommon.preferredVolume = EndianS16_BtoN(mvhdHeadCommon.preferredVolume);
    mvhdHeadCommon.reserved1 = EndianS16_BtoN(mvhdHeadCommon.reserved1);
    mvhdHeadCommon.preferredLong1 = EndianS32_BtoN(mvhdHeadCommon.preferredLong1);
    mvhdHeadCommon.preferredLong2 = EndianS32_BtoN(mvhdHeadCommon.preferredLong2);
	EndianMatrix_BtoN(&mvhdHeadCommon.matrix);
    mvhdHeadCommon.previewTime = EndianS32_BtoN(mvhdHeadCommon.previewTime);
    mvhdHeadCommon.previewDuration = EndianS32_BtoN(mvhdHeadCommon.previewDuration);
    mvhdHeadCommon.posterTime = EndianS32_BtoN(mvhdHeadCommon.posterTime);
    mvhdHeadCommon.selectionTime = EndianS32_BtoN(mvhdHeadCommon.selectionTime);
    mvhdHeadCommon.selectionDuration = EndianS32_BtoN(mvhdHeadCommon.selectionDuration);
    mvhdHeadCommon.currentTime = EndianS32_BtoN(mvhdHeadCommon.currentTime);
    mvhdHeadCommon.nextTrackID = EndianS32_BtoN(mvhdHeadCommon.nextTrackID);
	
	// Print atom contents non-required fields
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);
	atomprint("creationTime=\"%s\"\n", int64toxstr(mvhdHead.creationTime));
	atomprint("modificationTime=\"%s\"\n", int64toxstr(mvhdHead.modificationTime));
	atomprint("timeScale=\"%s\"\n", int64todstr(mvhdHead.timeScale));
	atomprint("duration=\"%s\"\n", int64todstr(mvhdHead.duration));
	atomprint("nextTrackID=\"%ld\"\n", mvhdHeadCommon.nextTrackID);
	atomprint("/>\n"); 

	// Check required field values
	FieldMustBe( mvhdHeadCommon.preferredRate, 0x00010000, "'mhvd' preferredRate must be 0x%lx not 0x%lx" );
	FieldMustBe( mvhdHeadCommon.preferredVolume, 0x0100, "'mhvd' preferredVolume must be 0x%lx not 0x%lx" );
	FieldMustBe( mvhdHeadCommon.reserved1, 0, "'mhvd' has a non-zero reserved field, should be %d is %d" );
	FieldMustBe( mvhdHeadCommon.reserved1, 0, "'mhvd' has a non-zero reserved field, should be %d is %d" );
	FieldMustBe( mvhdHeadCommon.preferredLong1, 0, "'mhvd' has a non-zero reserved field, should be %d is %d" );
	FieldMustBe( mvhdHeadCommon.preferredLong2, 0, "'mhvd' has a non-zero reserved field, should be %d is %d" );
	FieldMustBe( mvhdHeadCommon.previewTime, 0, "'mhvd' has a non-zero reserved field, should be %d is %d" );
	FieldMustBe( mvhdHeadCommon.previewDuration, 0, "'mhvd' has a non-zero reserved field, should be %d is %d" );
	FieldMustBe( mvhdHeadCommon.posterTime, 0, "'mhvd' has a non-zero reserved field, should be %d is %d" );
	FieldMustBe( mvhdHeadCommon.selectionTime, 0, "'mhvd' has a non-zero reserved field, should be %d is %d" );
	FieldMustBe( mvhdHeadCommon.selectionDuration, 0, "'mhvd' has a non-zero reserved field, should be %d is %d" );
	FieldMustBe( mvhdHeadCommon.currentTime, 0, "'mhvd' has a non-zero reserved field, should be %d is %d" );
		
	BAILIFERR( CheckMatrixForUnity( mvhdHeadCommon.matrix ) );

	// All done
	aoe->aoeflags |= kAtomValidated;

bail:
	return err;
}


//===============================================

typedef struct TrackHeaderCommonRecord {
    TimeValue               movieTimeOffset;            // reserved in mp4
	PriorityType			priority;		            // reserved in mp4
    SInt16					layer;			            // reserved in mp4
    SInt16					alternateGroup;	            // reserved in mp4

    SInt16					volume;						// 256 for audio, 0 otherwise
    SInt16					reserved;

    MatrixRecord			matrix;						// must be identity matrix
    Fixed					trackWidth;					// 320 for video, 0 otherwise
    Fixed					trackHeight;				// 240 for video, 0 otherwise
} TrackHeaderCommonRecord;

typedef struct TrackHeaderVers0Record {
    UInt32    				creationTime;
    UInt32    				modificationTime;
    UInt32					trackID;
    UInt32					reserved;
    TimeValue               duration;
} TrackHeaderVers0Record;

typedef struct TrackHeaderVers1Record {
    UInt64    				creationTime;
    UInt64    				modificationTime;
    UInt32            		trackID;
    UInt32					reserved;
    UInt64					duration;
} TrackHeaderVers1Record;
	
OSErr Validate_tkhd_Atom( atomOffsetEntry *aoe, void *refcon )
{
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;
	TrackHeaderVers1Record	tkhdHead;
	TrackHeaderCommonRecord	tkhdHeadCommon;
	TrackInfoRec			*tir = (TrackInfoRec*)refcon;

	
	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );
	

	// Get data based on version
	if (version == 0) {
		TrackHeaderVers0Record	tkhdHead0;
		BAILIFERR( GetFileData( aoe, &tkhdHead0, offset, sizeof(tkhdHead0), &offset ) );
		tkhdHead.creationTime = EndianU32_BtoN(tkhdHead0.creationTime);
		tkhdHead.modificationTime = EndianU32_BtoN(tkhdHead0.modificationTime);
		tkhdHead.trackID = EndianU32_BtoN(tkhdHead0.trackID);
		tkhdHead.reserved = EndianU32_BtoN(tkhdHead0.reserved);
		tkhdHead.duration = EndianU32_BtoN(tkhdHead0.duration);
		
		FieldMustBe( EndianU32_BtoN(tkhdHead0.reserved), 0, "'tkhd' reserved must be %d not %d" );
	} else if (version == 1) {
		BAILIFERR( GetFileData( aoe, &tkhdHead.creationTime, offset, sizeof(tkhdHead.creationTime), &offset ) );
		tkhdHead.creationTime = EndianU64_BtoN(tkhdHead.creationTime);
		BAILIFERR( GetFileData( aoe, &tkhdHead.modificationTime, offset, sizeof(tkhdHead.modificationTime), &offset ) );
		tkhdHead.modificationTime = EndianU64_BtoN(tkhdHead.modificationTime);
		BAILIFERR( GetFileData( aoe, &tkhdHead.trackID , offset, sizeof(tkhdHead.trackID ), &offset ) );
		tkhdHead.trackID = EndianU32_BtoN(tkhdHead.trackID);
		BAILIFERR( GetFileData( aoe, &tkhdHead.reserved , offset, sizeof(tkhdHead.reserved ), &offset ) );
		tkhdHead.reserved = EndianU32_BtoN(tkhdHead.reserved);
		BAILIFERR( GetFileData( aoe, &tkhdHead.duration, offset, sizeof(tkhdHead.duration), &offset ) );
		tkhdHead.duration = EndianU64_BtoN(tkhdHead.duration);
	} else {
		errprint("Track header is version other than 0 or 1\n");
		err = badAtomErr;
		goto bail;
	}
	BAILIFERR( GetFileData( aoe, &tkhdHeadCommon, offset, sizeof(tkhdHeadCommon), &offset ) );
	tkhdHeadCommon.movieTimeOffset = EndianU32_BtoN(tkhdHeadCommon.movieTimeOffset);
	tkhdHeadCommon.priority = EndianU32_BtoN(tkhdHeadCommon.priority);
	tkhdHeadCommon.layer = EndianS16_BtoN(tkhdHeadCommon.layer);
	tkhdHeadCommon.alternateGroup = EndianS16_BtoN(tkhdHeadCommon.alternateGroup);
	tkhdHeadCommon.volume = EndianS16_BtoN(tkhdHeadCommon.volume);
	tkhdHeadCommon.reserved = EndianS16_BtoN(tkhdHeadCommon.reserved);
	EndianMatrix_BtoN(&tkhdHeadCommon.matrix);
	tkhdHeadCommon.trackWidth = EndianS32_BtoN(tkhdHeadCommon.trackWidth);
	tkhdHeadCommon.trackHeight = EndianS32_BtoN(tkhdHeadCommon.trackHeight);
	
	// Print atom contents non-required fields
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);
	atomprint("creationTime=\"%s\"\n", int64toxstr(tkhdHead.creationTime));
	atomprint("modificationTime=\"%s\"\n", int64toxstr(tkhdHead.modificationTime));
	atomprint("trackID=\"%ld\"\n", tkhdHead.trackID);
	atomprint("duration=\"%s\"\n", int64todstr(tkhdHead.duration));
	atomprint("volume=\"%s\"\n", fixed16str(tkhdHeadCommon.volume));
	atomprint("width=\"%s\"\n", fixedU32str(tkhdHeadCommon.trackWidth));
	atomprint("height=\"%s\"\n", fixedU32str(tkhdHeadCommon.trackHeight));
	atomprint("/>\n"); 

	// Check required field values
	// else FieldMustBe( flags, 1, "'tkhd' flags must be 1" );
	if ((flags & 7) != flags) errprint("Tkhd flags 0x%X other than 1,2 or 4 set\n", flags);
	if (flags == 0) warnprint( "WARNING: 'tkhd' flags == 0 (OK in a hint track)\n", flags );
	if (tkhdHead.duration == 0) warnprint( "WARNING: 'tkhd' duration == 0, track may be considered empty\n", flags );


	FieldMustBe( tkhdHeadCommon.movieTimeOffset, 0, "'tkhd' movieTimeOffset must be %d not %d" );
	FieldMustBe( tkhdHeadCommon.priority, 0, "'tkhd' priority must be %d not %d" );
	FieldMustBe( tkhdHeadCommon.layer, 0, "'tkhd' layer must be %d not %d" );
	FieldMustBe( tkhdHeadCommon.alternateGroup, 0, "'tkhd' alternateGroup must be %d not %d" );
	FieldMustBe( tkhdHeadCommon.reserved, 0, "'tkhd' reserved must be %d not %d" );
	
	// ееее CHECK for audio/video
	{
		FieldMustBeOneOf2( tkhdHeadCommon.volume, SInt16, "'tkhd' volume must be set to one of ", (0, 0x0100) );
		if( vg.majorBrand == brandtype_mp41 ){
			FieldMustBeOneOf2( tkhdHeadCommon.trackWidth, Fixed, "'tkhd' trackWidth must be set to one of ", (0, (320L << 16)) );
			FieldMustBeOneOf2( tkhdHeadCommon.trackHeight, Fixed, "'tkhd' trackHeight must be set to one of ", (0, (240L << 16)) );
		}
	}

		// save off some of the fields in the rec
	if (tir != NULL) {
		tir->trackID = tkhdHead.trackID;
		tir->trackWidth = tkhdHeadCommon.trackWidth;
		tir->trackHeight = tkhdHeadCommon.trackHeight;
	}
	else errprint("Internal error -- Track ID %d not recorded\n",tkhdHead.trackID);

		BAILIFERR( CheckMatrixForUnity( tkhdHeadCommon.matrix ) );


	// All done
	aoe->aoeflags |= kAtomValidated;

bail:
	return err;
}


//==========================================================================================

typedef struct MediaHeaderCommonRecord {
	UInt16					language; 		// high-bit = 0, packed ISO-639-2/T language code (int(5)[3])
	UInt16					quality;		// must be 0 for mp4
} MediaHeaderCommonRecord;

typedef struct MediaHeaderVers0Record {
    UInt32    				creationTime;
    UInt32    				modificationTime;
    UInt32	            	timescale;
    UInt32	            	duration;
} MediaHeaderVers0Record;

typedef struct MediaHeaderVers1Record {
    UInt64    				creationTime;
    UInt64    				modificationTime;
    UInt32	            	timescale;
    UInt64					duration;
} MediaHeaderVers1Record;
	

OSErr Validate_mdhd_Atom( atomOffsetEntry *aoe, void *refcon )
{
	TrackInfoRec *tir = (TrackInfoRec *)refcon;
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;
	MediaHeaderVers1Record	mdhdHead;
	MediaHeaderCommonRecord	mdhdHeadCommon;
	
	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );

	// Get data based on version
	if (version == 0) {
		MediaHeaderVers0Record	mdhdHead0;
		BAILIFERR( GetFileData( aoe, &mdhdHead0, offset, sizeof(mdhdHead0), &offset ) );
		mdhdHead.creationTime = EndianU32_BtoN(mdhdHead0.creationTime);
		mdhdHead.modificationTime = EndianU32_BtoN(mdhdHead0.modificationTime);
		mdhdHead.timescale = EndianU32_BtoN(mdhdHead0.timescale);
		mdhdHead.duration = EndianU32_BtoN(mdhdHead0.duration);
	} else if (version == 1) {
		BAILIFERR( GetFileData( aoe, &mdhdHead.creationTime, offset, sizeof(mdhdHead.creationTime), &offset ) );
		mdhdHead.creationTime = EndianU64_BtoN(mdhdHead.creationTime);
		BAILIFERR( GetFileData( aoe, &mdhdHead.modificationTime, offset, sizeof(mdhdHead.modificationTime), &offset ) );
		mdhdHead.modificationTime = EndianU64_BtoN(mdhdHead.modificationTime);
		BAILIFERR( GetFileData( aoe, &mdhdHead.timescale, offset, sizeof(mdhdHead.timescale), &offset ) );
		mdhdHead.timescale = EndianU32_BtoN(mdhdHead.timescale);
		BAILIFERR( GetFileData( aoe, &mdhdHead.duration, offset, sizeof(mdhdHead.duration), &offset ) );
		mdhdHead.duration = EndianU64_BtoN(mdhdHead.duration);
	} else {
		errprint("Media header is version other than 0 or 1\n");
		err = badAtomErr;
		goto bail;
	}
	tir->mediaTimeScale = mdhdHead.timescale;
	tir->mediaDuration = mdhdHead.duration;

	BAILIFERR( GetFileData( aoe, &mdhdHeadCommon, offset, sizeof(mdhdHeadCommon), &offset ) );
	mdhdHeadCommon.language = EndianU16_BtoN(mdhdHeadCommon.language); 
	mdhdHeadCommon.quality = EndianU16_BtoN(mdhdHeadCommon.quality); 
	
	// Print atom contents non-required fields
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);
	atomprint("creationTime=\"%s\"\n", int64toxstr(mdhdHead.creationTime));
	atomprint("modificationTime=\"%s\"\n", int64toxstr(mdhdHead.modificationTime));
	atomprint("timescale=\"%s\"\n", int64todstr(mdhdHead.timescale));
	atomprint("duration=\"%s\"\n", int64todstr(mdhdHead.duration));
	atomprint("language=\"%s\"\n", langtodstr(mdhdHeadCommon.language));
	if (mdhdHeadCommon.language==0) warnprint("Warning: Media Header language code of 0 not strictly legit -- 'und' preferred\n");
	
	atomprint("/>\n"); 

	// Check required field values
	FieldMustBe( flags, 0, "'mdvd' flags must be %d not %d" );
	FieldMustBe( mdhdHeadCommon.quality, 0, "'mdhd' quality (reserved in mp4) must be %d not %d" );
	FieldCheck( (mdhdHead.duration > 0), "'mdhd' duration must be > 0" );

	// All done
	aoe->aoeflags |= kAtomValidated;

bail:
	return err;

}

//==========================================================================================

typedef struct HandlerInfoRecord {
    UInt32	componentType;
    UInt32	componentSubType;
    UInt32	componentManufacturer;
    UInt32	componentFlags;
    UInt32	componentFlagsMask;
    char    Name[1];
} HandlerInfoRecord;

OSErr Get_mdia_hdlr_mediaType( atomOffsetEntry *aoe, TrackInfoRec *tir )
{
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;
	HandlerInfoRecord	hdlrInfo;

	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );
	BAILIFERR( GetFileData( aoe, &hdlrInfo, offset, fieldOffset(HandlerInfoRecord, Name), &offset ) );
	tir->mediaType = EndianU32_BtoN(hdlrInfo.componentSubType);
bail:
	return err;
}

OSErr Validate_mdia_hdlr_Atom( atomOffsetEntry *aoe, void *refcon )
{
	TrackInfoRec *tir = (TrackInfoRec *)refcon;
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;
	HandlerInfoRecord	hdlrInfo;
	char *nameP;

	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);
	FieldMustBe( version, 0, "version must be %d not %d" );
	FieldMustBe( flags, 0, "flags must be %d not %d" );

	// Get Handler Info (minus name) 
	BAILIFERR( GetFileData( aoe, &hdlrInfo, offset, fieldOffset(HandlerInfoRecord, Name), &offset ) );
	hdlrInfo.componentType = EndianU32_BtoN(hdlrInfo.componentType);
	hdlrInfo.componentSubType = EndianU32_BtoN(hdlrInfo.componentSubType);
	hdlrInfo.componentFlags = EndianU32_BtoN(hdlrInfo.componentFlags);
	hdlrInfo.componentFlagsMask = EndianU32_BtoN(hdlrInfo.componentFlagsMask);
	hdlrInfo.componentFlags = EndianU32_BtoN(hdlrInfo.componentFlags);
	hdlrInfo.componentFlags = EndianU32_BtoN(hdlrInfo.componentFlags);

	// Remember info in the refcon
	if (vg.print_atompath) {
		fprintf(stdout,"\t\tHandler subtype = '%s'\n", ostypetostr(hdlrInfo.componentSubType));
	}
	tir->mediaType = hdlrInfo.componentSubType;
	atomprint("handler_type=\"%s\"\n", ostypetostr(hdlrInfo.componentSubType));
	
	// Get Handler Info Name
	BAILIFERR( GetFileCString( aoe, &nameP, offset, aoe->maxOffset - offset, &offset ) );
	atomprint("name=\"%s\"\n", nameP);

	// Check required field values
	FieldMustBe( hdlrInfo.componentType, 0, "'hdlr' componentType (reserved in mp4) must be %d not 0x%lx" );
	FieldMustBe( hdlrInfo.componentManufacturer, 0, "'hdlr' componentManufacturer (reserved in mp4) must be %d not 0x%lx" );
	FieldMustBe( hdlrInfo.componentFlags, 0, "'hdlr' componentFlags (reserved in mp4) must be %d not 0x%lx" );
	FieldMustBe( hdlrInfo.componentFlagsMask, 0, "'hdlr' componentFlagsMask (reserved in mp4) must be %d not 0x%lx" );

		FieldMustBeOneOf10( hdlrInfo.componentSubType, OSType, 
			"'hdlr' handler type must be be one of ", 
			('odsm', 'crsm', 'sdsm', 'vide', 'soun', 'm7sm', 'ocsm', 'ipsm', 'mjsm', 'hint') );

	
	// All done
	atomprint("/>\n"); 
	aoe->aoeflags |= kAtomValidated;

bail:
	return err;
}

OSErr Validate_hdlr_Atom( atomOffsetEntry *aoe, void *refcon )
{
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;
	HandlerInfoRecord	hdlrInfo;
	char *nameP;

	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);
	FieldMustBe( version, 0, "version must be %d not %d" );
	FieldMustBe( flags, 0, "flags must be %d not %d" );

	// Get Handler Info (minus name) 
	BAILIFERR( GetFileData( aoe, &hdlrInfo, offset, fieldOffset(HandlerInfoRecord, Name), &offset ) );
	hdlrInfo.componentType = EndianU32_BtoN(hdlrInfo.componentType);
	hdlrInfo.componentSubType = EndianU32_BtoN(hdlrInfo.componentSubType);
	hdlrInfo.componentFlags = EndianU32_BtoN(hdlrInfo.componentFlags);
	hdlrInfo.componentFlagsMask = EndianU32_BtoN(hdlrInfo.componentFlagsMask);
	hdlrInfo.componentFlags = EndianU32_BtoN(hdlrInfo.componentFlags);
	hdlrInfo.componentFlags = EndianU32_BtoN(hdlrInfo.componentFlags);

	// Remember info in the refcon
	if (vg.print_atompath) {
		fprintf(stdout,"\t\tHandler subtype = '%s'\n", ostypetostr(hdlrInfo.componentSubType));
	}
	atomprint("handler_type=\"%s\"\n", ostypetostr(hdlrInfo.componentSubType));
	
	// Get Handler Info Name
	BAILIFERR( GetFileCString( aoe, &nameP, offset, aoe->maxOffset - offset, &offset ) );
	atomprint("name=\"%s\"\n", nameP);

	// Check required field values
	FieldMustBe( hdlrInfo.componentType, 0, "'hdlr' componentType (reserved in mp4) must be %d not 0x%lx" );
	FieldMustBe( hdlrInfo.componentManufacturer, 0, "'hdlr' componentManufacturer (reserved in mp4) must be %d not 0x%lx" );
	FieldMustBe( hdlrInfo.componentFlags, 0, "'hdlr' componentFlags (reserved in mp4) must be %d not 0x%lx" );
	FieldMustBe( hdlrInfo.componentFlagsMask, 0, "'hdlr' componentFlagsMask (reserved in mp4) must be %d not 0x%lx" );
	
	// All done
	atomprint("/>\n"); 
	aoe->aoeflags |= kAtomValidated;

bail:
	return err;
}

//==========================================================================================

typedef struct VideoMediaInfoHeader {
    UInt16	graphicsMode;               /* for QD - transfer mode */
    UInt16	opColorRed;                 /* opcolor for transfer mode */
    UInt16	opColorGreen;
    UInt16	opColorBlue;
} VideoMediaInfoHeader;

OSErr Validate_vmhd_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;
	VideoMediaInfoHeader	vmhdInfo;

	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );

	// Get data 
	BAILIFERR( GetFileData( aoe, &vmhdInfo, offset, sizeof(vmhdInfo), &offset ) );
    vmhdInfo.graphicsMode = EndianU16_BtoN(vmhdInfo.graphicsMode);
	vmhdInfo.opColorRed = EndianU16_BtoN(vmhdInfo.opColorRed);
	vmhdInfo.opColorGreen = EndianU16_BtoN(vmhdInfo.opColorGreen);
	vmhdInfo.opColorBlue = EndianU16_BtoN(vmhdInfo.opColorBlue);
	
	// Print atom contents non-required fields
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);
	atomprint("/>\n"); 

	// Check required field values
	FieldMustBe( version, 0, "'vmhd' version must be %d not %d" );
	FieldMustBe( flags, 1, "'vmhd' flags must be %d not 0x%lx" );
	FieldMustBe( vmhdInfo.graphicsMode, 0, "'vmhd' graphicsMode (reserved in mp4) must be %d not 0x%lx" );
	FieldMustBe( vmhdInfo.opColorRed,   0, "'vmhd' opColorRed   (reserved in mp4) must be %d not 0x%lx" );
	FieldMustBe( vmhdInfo.opColorGreen, 0, "'vmhd' opColorGreen (reserved in mp4) must be %d not 0x%lx" );
	FieldMustBe( vmhdInfo.opColorBlue,  0, "'vmhd' opColorBlue  (reserved in mp4) must be %d not 0x%lx" );

	// All done
	aoe->aoeflags |= kAtomValidated;

bail:
	return err;
}


//==========================================================================================

typedef struct SoundMediaInfoHeader {
    UInt16	balance;
    UInt16	rsrvd;
} SoundMediaInfoHeader;

OSErr Validate_smhd_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;
	SoundMediaInfoHeader	smhdInfo;

	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );

	// Get data 
	BAILIFERR( GetFileData( aoe, &smhdInfo, offset, sizeof(smhdInfo), &offset ) );
	smhdInfo.balance = EndianU16_BtoN(smhdInfo.balance);
	smhdInfo.rsrvd = EndianU16_BtoN(smhdInfo.rsrvd);

	// Print atom contents non-required fields
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);
	atomprint("/>\n"); 

	// Check required field values
	FieldMustBe( flags, 0, "'smhd' flags must be %d not 0x%lx" );
	FieldMustBe( smhdInfo.balance, 0, "'smhd' balance (reserved in mp4) must be %d not %d" );
	FieldMustBe( smhdInfo.rsrvd,   0, "'smhd' rsrvd must be %d not 0x%lx" );

	// All done
	aoe->aoeflags |= kAtomValidated;

bail:
	return err;
}


//==========================================================================================

typedef struct HintMediaInfoHeader {
    UInt16	maxPDUsize;
    UInt16	avgPDUsize;
    UInt32	maxbitrate;
    UInt32	avgbitrate;
    UInt32	slidingavgbitrate;
} HintMediaInfoHeader;

OSErr Validate_hmhd_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;
	HintMediaInfoHeader	hmhdInfo;

	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );

	// Get data 
	BAILIFERR( GetFileData( aoe, &hmhdInfo, offset, sizeof(hmhdInfo), &offset ) );
	hmhdInfo.maxPDUsize = EndianU16_BtoN(hmhdInfo.maxPDUsize);
	hmhdInfo.avgPDUsize = EndianU16_BtoN(hmhdInfo.avgPDUsize);
	hmhdInfo.maxbitrate = EndianU32_BtoN(hmhdInfo.maxbitrate);
	hmhdInfo.avgbitrate = EndianU32_BtoN(hmhdInfo.avgbitrate);
	hmhdInfo.slidingavgbitrate = EndianU32_BtoN(hmhdInfo.slidingavgbitrate);
	
	// Print atom contents non-required fields
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);
	atomprint("maxPDUsize=\"%ld\"\n", hmhdInfo.maxPDUsize);
	atomprint("avgPDUsize=\"%ld\"\n", hmhdInfo.avgPDUsize);
	atomprint("maxbitrate=\"%ld\"\n", hmhdInfo.maxbitrate);
	atomprint("avgbitrate=\"%ld\"\n", hmhdInfo.avgbitrate);
	atomprint("slidingavgbitrate=\"%ld\"\n", hmhdInfo.slidingavgbitrate);
	atomprint("/>\n"); 

	// Check required field values
	FieldMustBe( flags, 0, "'hmdh' flags must be %d not 0x%lx" );

	// All done
	aoe->aoeflags |= kAtomValidated;

bail:
	return err;
}


//==========================================================================================

OSErr Validate_nmhd_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;

	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );

	// There is no data 
	
	// Print atom contents non-required fields
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);
	atomprint("/>\n"); 

//еееее need to check for underrun

	// Check required field values
	FieldMustBe( flags, 0, "'nmhd' flags must be %d not 0x%lx" );

	// All done
	aoe->aoeflags |= kAtomValidated;

bail:
	return err;
}


//==========================================================================================

OSErr Validate_mp4s_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;

	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );

	// There is no data
		
	// Print atom contents non-required fields
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);

	atomprint("/>\n"); 

	// Check required field values
	FieldMustBe( flags, 0, "'mp4s' flags must be %d not 0x%lx" );

	// All done
	aoe->aoeflags |= kAtomValidated;

bail:
	return err;
}

//==========================================================================================

OSErr Validate_url_Entry( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;
	char *locationP = nil;

	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );

	// Get data 
	if (flags & 1) {
		// no more data
	} else {
		BAILIFERR( GetFileCString( aoe, &locationP, offset, aoe->maxOffset - offset, &offset ) );
	}
	
		
	// Print atom contents non-required fields
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);
	if (locationP) {
		atomprint("location=\"%s\"\n", locationP);
	}
	atomprint("/>\n"); 

	// Check required field values
//еее	FieldMustBe( flags, 0, "'mp4s' flags must be 0" );
//еее   need to check that the atom has ended.

	// All done
	aoe->aoeflags |= kAtomValidated;

bail:
	return err;
}

//==========================================================================================

OSErr Validate_urn_Entry( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;
	char *nameP = nil;
	char *locationP = nil;

	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );

	// Get data 
	// name is required
	BAILIFERR( GetFileCString( aoe, &nameP, offset, aoe->maxOffset - offset, &offset ) );
	if (offset >= (aoe->offset + aoe->size)) {
		BAILIFERR( GetFileCString( aoe, &locationP, offset, aoe->maxOffset - offset, &offset ) );
	}
		
	// Print atom contents non-required fields
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);
	atomprint("name=\"%s\"\n", nameP);
	if (locationP) {
		atomprint("location=\"%s\"\n", locationP);
	}
	atomprint("/>\n"); 

	// Check required field values
//еее	FieldMustBe( flags, 0, "'mp4s' flags must be 0" );
//еее   need to check that the atom has ended.

	// All done
	aoe->aoeflags |= kAtomValidated;

bail:
	return err;
}

//==========================================================================================



OSErr Validate_dref_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;
	UInt32 entryCount;

	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );

	// Get data 
	BAILIFERR( GetFileDataN32( aoe, &entryCount, offset, &offset ) );
	
	// Print atom contents non-required fields
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);
	atomprint("entryCount=\"%ld\"\n", entryCount);
	atomprint(">\n"); //vg.tabcnt++; 

	// Check required field values
	FieldMustBe( flags, 0, "'dref' flags must be %d not 0x%lx" );

	//need to validate url urn
	{
		UInt64 minOffset, maxOffset;
		atomOffsetEntry *entry;
		long cnt;
		atomOffsetEntry *list;
		int i;
		
		minOffset = offset;
		maxOffset = aoe->offset + aoe->size;
		
		BAILIFERR( FindAtomOffsets( aoe, minOffset, maxOffset, &cnt, &list ) );
		
		for (i = 0; i < cnt; i++) {
			entry = &list[i];
			
			atomprint("<%s",ostypetostr(entry->type)); vg.tabcnt++;
			
			switch( entry->type ) {
				case 'url ':
					Validate_url_Entry( entry, refcon );
					break;

				case 'urn ':
					Validate_urn_Entry( entry, refcon );
					break;
					
				default:
				// ее should warn
					warnprint("WARNING: unknown/unexpected dref entry '%s'\n",ostypetostr(entry->type));
					atomprint("???? />\n");
					break;
			}
			--vg.tabcnt; 

		}

	}
	
//	vg.tabcnt--;
	
	// All done
	aoe->aoeflags |= kAtomValidated;

bail:
	return err;
}

//==========================================================================================

OSErr Validate_stts_Atom( atomOffsetEntry *aoe, void *refcon )
{
	TrackInfoRec *tir = (TrackInfoRec *)refcon;
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;
	UInt32 entryCount;
	TimeToSampleNum *listP = NULL;
	UInt32 listSize;
	UInt32 i;
	UInt32 numSamples = 0;
	UInt64 totalDuration = 0;
	Boolean lastSampleDurationIsZero = false;
	char 	tempStr1[32];
	char 	tempStr2[32];

	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );

	// Get data 
	BAILIFERR( GetFileDataN32( aoe, &entryCount, offset, &offset ) );
		//  adding 1 to entryCount to make this 1 based array
	listSize = entryCount * sizeof(TimeToSampleNum);
	BAILIFNULL( listP = malloc(listSize + sizeof(TimeToSampleNum)), allocFailedErr );
	BAILIFERR( GetFileData( aoe, &listP[1], offset, listSize, &offset ) );
	listP[0].sampleCount = 0; listP[0].sampleDuration = 0;
	for ( i = 1; i <= entryCount; i++ ) {
		listP[i].sampleCount = EndianS32_BtoN(listP[i].sampleCount);
		listP[i].sampleDuration = EndianS32_BtoN(listP[i].sampleDuration);
	}

	// Print atom contents non-required fields
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);
	atomprint("entryCount=\"%ld\"\n", entryCount);
	atomprint("/>\n");
	vg.tabcnt++;
		//  changes to i stuff where needed to make it 1 based
	for ( i = 1; i <= entryCount; i++ ) {
		atomprintdetailed("<sttsEntry sampleCount=\"%d\" sampleDelta/duration=\"%d\" />\n", listP[i].sampleCount, listP[i].sampleDuration);
		if (!listP[i].sampleDuration) {
			if (i == (entryCount)) {
				lastSampleDurationIsZero = true;
			} else {
				errprint("You can't have a zero duration other than last in the stts TimeToSample table\n");
			}
		}
		numSamples += listP[i].sampleCount;
		totalDuration += listP[i].sampleCount * listP[i].sampleDuration;
	}
	--vg.tabcnt;

	// Check required field values
	FieldMustBe( flags, 0, "'stts' flags must be %d not 0x%lx" );

	if (lastSampleDurationIsZero) {
		if ((tir->mediaDuration) && (totalDuration > tir->mediaDuration)) {
			errprint("The last TimeToSample sample duration is zero, but the total duration (%s)"
					 " described by the table is greater than the 'mdhd' mediaDuration (%s)\n",
					 int64todstr_r(totalDuration, tempStr1), int64todstr_r(tir->mediaDuration, tempStr2));
		} else if ((tir->mediaDuration) && (totalDuration == tir->mediaDuration)) {
			warnprint("Warning: The last TimeToSample sample duration is zero, but the total duration (%s)"
					 " described by the table is equal to the 'mdhd' mediaDuration (%s)\n",
					 int64todstr_r(totalDuration, tempStr1), int64todstr_r(tir->mediaDuration, tempStr2));
		}
	}
	

	// All done
	aoe->aoeflags |= kAtomValidated;

bail:
	tir->timeToSample = listP;

	tir->timeToSampleSampleCnt = numSamples;
	tir->timeToSampleDuration = totalDuration;
	tir->timeToSampleEntryCnt = entryCount;
	
	return err;
}

//==========================================================================================


typedef struct CompositionTimeToSampleNum {
    long             sampleCount;
    TimeValue        sampleOffset;
} CompositionTimeToSampleNum;

OSErr Validate_ctts_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;
	UInt32 entryCount;
	UInt32 totalcount;
	UInt32 allzero;
	CompositionTimeToSampleNum *listP;
	UInt32 listSize;
	UInt32 i;
	
	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );

	// Get data 
	BAILIFERR( GetFileDataN32( aoe, &entryCount, offset, &offset ) );
	listSize = entryCount * sizeof(TimeToSampleNum);
	BAILIFNIL( listP = malloc(listSize), allocFailedErr );
	BAILIFERR( GetFileData( aoe, listP, offset, listSize, &offset ) );
	for ( i = 0; i < entryCount; i++ ) {
		listP[i].sampleCount = EndianS32_BtoN(listP[i].sampleCount);
		listP[i].sampleOffset = EndianS32_BtoN(listP[i].sampleOffset);
	}
	
	// Print atom contents non-required fields
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);
	atomprint("entryCount=\"%ld\"\n", entryCount);
	atomprint("/>\n");
	vg.tabcnt++;
	
	totalcount = 0;  allzero = 1;
	
	for ( i = 0; i < entryCount; i++ ) {
		atomprintdetailed("<cttsEntry sampleCount=\"%d\" sampleDelta/duration=\"%d\" />\n", listP[i].sampleCount, listP[i].sampleOffset);
		if (listP[i].sampleOffset < 0) {
			errprint("You can't have a negative offset in the ctts table\n");
		}
		totalcount += listP[i].sampleCount;
		if (listP[i].sampleOffset != 0) allzero = 0;
	}
	
	if (totalcount == 0) warnprint("WARNING: CTTS atom has no entries so is un-needed\n");
	if (allzero == 1) warnprint("WARNING: CTTS atom has no entry with a non-zero offset so is un-needed\n");
	--vg.tabcnt;

	// Check required field values
	FieldMustBe( flags, 0, "'ctts' flags must be %d not 0x%lx" );

	// All done
	aoe->aoeflags |= kAtomValidated;

bail:
	return err;
}

//==========================================================================================


OSErr Validate_stsz_Atom( atomOffsetEntry *aoe, void *refcon )
{
	TrackInfoRec *tir = (TrackInfoRec *)refcon;
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;
	UInt32 entryCount;
	UInt32 sampleSize;
	SampleSizeRecord *listP;
	UInt32 listSize;
	UInt32 i;
	
	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );

	// Get data 
	BAILIFERR( GetFileDataN32( aoe, &sampleSize, offset, &offset ) );
	BAILIFERR( GetFileDataN32( aoe, &entryCount, offset, &offset ) );
	if ((sampleSize == 0) && entryCount) {
		listSize = entryCount * sizeof(SampleSizeRecord);
			// 1 based array
		BAILIFNIL( listP = malloc(listSize + sizeof(SampleSizeRecord)), allocFailedErr );
		BAILIFERR( GetFileData( aoe, &listP[1], offset, listSize, &offset ) );
		for ( i = 1; i <= entryCount; i++ ) {
			listP[i].sampleSize = EndianS32_BtoN(listP[i].sampleSize);
		}
	}
	
	// Print atom contents non-required fields
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);
	atomprint("sampleSize=\"%ld\"\n", sampleSize);
	atomprint("entryCount=\"%ld\"\n", entryCount);
	atomprint("/>\n");
	if ((sampleSize == 0) && entryCount) {
		vg.tabcnt++;
		listP[0].sampleSize = 0;
		for ( i = 1; i <= entryCount; i++ ) {
			atomprintdetailed("<stszEntry sampleSize=\"%d\" />\n", listP[i].sampleSize);
			if (listP[i].sampleSize == 0) {
				errprint("You can't have a zero sample size in stsz\n");
			}
		}
		--vg.tabcnt;
	}
	
	// Check required field values
	FieldMustBe( flags, 0, "'stsz' flags must be %d not 0x%lx" );

	// All done
	aoe->aoeflags |= kAtomValidated;
	tir->sampleSizeEntryCnt = entryCount;
	tir->singleSampleSize = sampleSize;
	tir->sampleSize = listP;
	
bail:
	return err;
}

OSErr Validate_stz2_Atom( atomOffsetEntry *aoe, void *refcon )
{
	TrackInfoRec *tir = (TrackInfoRec *)refcon;
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;
	UInt32 entryCount;
	UInt32 temp;
	UInt8 fieldSize;
	SampleSizeRecord *listP;
	UInt32 listSize;
	UInt32 i;
	
	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );

	// Get data 
	BAILIFERR( GetFileData( aoe, &temp, offset, 3, &offset ) );
	BAILIFERR( GetFileData( aoe, &fieldSize, offset, 1, &offset ) );
	BAILIFERR( GetFileDataN32( aoe, &entryCount, offset, &offset ) );
	listSize = entryCount * sizeof(SampleSizeRecord);
		// 1 based array + room for one over for the 4-bit case loop
	BAILIFNIL( listP = malloc(listSize + sizeof(SampleSizeRecord) + sizeof(SampleSizeRecord)), allocFailedErr );
	
	if (entryCount) switch (fieldSize) {
		case 4:
			for (i=0; i<((entryCount+1)/2); i++) {
				UInt8 theSize;
				BAILIFERR( GetFileData( aoe, &theSize, offset, 1, &offset ) );
				listP[i*2 + 1].sampleSize = theSize >> 4;
				listP[i*2 + 2].sampleSize = theSize & 0x0F;
			}
			break;
		case 8:
			for (i=1; i<=entryCount; i++) {
				UInt8 theSize;
				BAILIFERR( GetFileData( aoe, &theSize, offset, 1, &offset ) );
				listP[i].sampleSize = theSize;
			}
			break;
		case 16:
			for (i=1; i<=entryCount; i++) {
				UInt16 theSize;
				BAILIFERR( GetFileDataN16( aoe, &theSize, offset, &offset ) );
				listP[i].sampleSize = theSize;
			}
			break;
		default: errprint("You can't have a field size of %d in stz2\n", fieldSize);
	}
	
	// Print atom contents non-required fields
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);
	atomprint("fieldSize=\"%ld\"\n", fieldSize);
	atomprint("entryCount=\"%ld\"\n", entryCount);
	atomprint("/>\n");
	if (entryCount) {
		vg.tabcnt++;
		listP[0].sampleSize = 0;
		for ( i = 1; i <= entryCount; i++ ) {
			atomprintdetailed("<stz2Entry sampleSize=\"%d\" />\n", listP[i].sampleSize);
			if (listP[i].sampleSize == 0) {
				errprint("You can't have a zero sample size in stz2\n");
			}
		}
		--vg.tabcnt;
	}
	
	// Check required field values
	FieldMustBe( flags, 0, "'stz2' flags must be %d not 0x%lx" );

	// All done
	aoe->aoeflags |= kAtomValidated;
	tir->sampleSizeEntryCnt = entryCount;
	tir->singleSampleSize = 0;
	tir->sampleSize = listP;
	
bail:
	return err;
}

//==========================================================================================

OSErr Validate_stsc_Atom( atomOffsetEntry *aoe, void *refcon )
{
	TrackInfoRec *tir = (TrackInfoRec *)refcon;
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;
	UInt32 entryCount;
	SampleToChunk *listP;
	UInt32 listSize;
	UInt32 i;
	UInt32 sampleToChunkSampleSubTotal = 0;		// total accounted for all but last entry
	
	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );

	// Get data 
	BAILIFERR( GetFileDataN32( aoe, &entryCount, offset, &offset ) );
	listSize = entryCount * sizeof(SampleToChunk);
			// 1 based array
	BAILIFNIL( listP = malloc(listSize + sizeof(SampleToChunk)), allocFailedErr );
	BAILIFERR( GetFileData( aoe, &listP[1], offset, listSize, &offset ) );
	for ( i = 1; i <= entryCount; i++ ) {
		listP[i].firstChunk = EndianU32_BtoN(listP[i].firstChunk);
		listP[i].samplesPerChunk = EndianU32_BtoN(listP[i].samplesPerChunk);
		listP[i].sampleDescriptionIndex = EndianU32_BtoN(listP[i].sampleDescriptionIndex);
		if (i > 1) {
			sampleToChunkSampleSubTotal += 
				( listP[i].firstChunk - listP[i-1].firstChunk )
					* ( listP[i-1].samplesPerChunk );
		}
	}

	// Print atom contents non-required fields
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);
	atomprint("entryCount=\"%ld\"\n", entryCount);
	atomprint("/>\n");
	vg.tabcnt++;
	listP[0].firstChunk = listP[0].samplesPerChunk = listP[0].sampleDescriptionIndex = 0;
	for ( i = 1; i <= entryCount; i++ ) {
		atomprintdetailed("<stscEntry firstChunk=\"%d\" samplesPerChunk=\"%d\" sampleDescriptionIndex=\"%d\" />\n", 
			listP[i].firstChunk, listP[i].samplesPerChunk, listP[i].sampleDescriptionIndex);
		
	}
	--vg.tabcnt;

	// Check required field values
	FieldMustBe( flags, 0, "'stsc' flags must be %d not 0x%lx" );

	// All done
	aoe->aoeflags |= kAtomValidated;
	
	tir->sampleToChunkEntryCnt = entryCount;
	tir->sampleToChunk = listP;
	tir->sampleToChunkSampleSubTotal = sampleToChunkSampleSubTotal;		// total accounted for all but last entry
	
bail:
	return err;
}

//==========================================================================================

OSErr Validate_stco_Atom( atomOffsetEntry *aoe, void *refcon )
{
	TrackInfoRec *tir = (TrackInfoRec *)refcon;
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;
	UInt32 entryCount;
	ChunkOffsetRecord *listP;
	ChunkOffset64Record *list64P;
	UInt32 listSize;
	UInt32 i;


	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );

	// Get data 
	BAILIFERR( GetFileDataN32( aoe, &entryCount, offset, &offset ) );
	listSize = entryCount * sizeof(ChunkOffsetRecord);
			// 1 based array
	BAILIFNIL( listP = malloc(listSize + sizeof(ChunkOffsetRecord)), allocFailedErr );
			// 1 based array
	BAILIFNIL( list64P = malloc((entryCount + 1) * sizeof(ChunkOffset64Record)), allocFailedErr );
	BAILIFERR( GetFileData( aoe, &listP[1], offset, listSize, &offset ) );
	for ( i = 1; i <= entryCount; i++ ) {
		listP[i].chunkOffset = EndianU32_BtoN(listP[i].chunkOffset);
	}

	
	// Print atom contents non-required fields
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);
	atomprint("entryCount=\"%ld\"\n", entryCount);




	atomprint("/>\n");
	vg.tabcnt++;
	list64P[0].chunkOffset = listP[0].chunkOffset = 0;
	for ( i = 1; i <= entryCount; i++ ) {
		atomprintdetailed("<stcoEntry chunkOffset=\"%ld\" />\n", listP[i].chunkOffset);
		if (listP[i].chunkOffset == 0) {
			errprint("You can't have a zero sample size in stco\n");
		}
		list64P[i].chunkOffset = listP[i].chunkOffset;
	}
	--vg.tabcnt;

	// Check required field values
	FieldMustBe( flags, 0, "'stco' flags must be %d not 0x%lx" );

	// All done
	aoe->aoeflags |= kAtomValidated;
	tir->chunkOffsetEntryCnt = entryCount;
	tir->chunkOffset = list64P;
	free(listP);
	
bail:
	return err;
}

//==========================================================================================

OSErr Validate_co64_Atom( atomOffsetEntry *aoe, void *refcon )
{
	TrackInfoRec *tir = (TrackInfoRec *)refcon;
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;
	UInt32 entryCount;
	ChunkOffset64Record *listP;
	UInt32 listSize;
	UInt32 i;
	
	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );

	// Get data 
	BAILIFERR( GetFileDataN32( aoe, &entryCount, offset, &offset ) );
	listSize = entryCount * sizeof(ChunkOffset64Record);
		// 1 based table
	BAILIFNIL( listP = malloc(listSize + sizeof(ChunkOffset64Record)), allocFailedErr );
	BAILIFERR( GetFileData( aoe, &listP[1], offset, listSize, &offset ) );
	for ( i = 1; i <= entryCount; i++ ) {
		listP[i].chunkOffset = EndianU64_BtoN(listP[i].chunkOffset);
	}
	
	// Print atom contents non-required fields
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);
	atomprint("entryCount=\"%ld\"\n", entryCount);
	atomprint("/>\n");
	vg.tabcnt++;
	listP[0].chunkOffset = 0;
	for ( i = 1; i <= entryCount; i++ ) {
		atomprintdetailed("<stcoEntry chunkOffset=\"%s\" />\n", int64todstr(listP[i].chunkOffset));
		if (listP[i].chunkOffset == 0) {
			errprint("You can't have a zero sample size in stsz\n");
		}
	}
	--vg.tabcnt;

	// Check required field values
	FieldMustBe( flags, 0, "'stco' flags must be %d not 0x%lx" );

	// All done
	aoe->aoeflags |= kAtomValidated;
	tir->chunkOffsetEntryCnt = entryCount;
	tir->chunkOffset = listP;
	
bail:
	return err;
}

//==========================================================================================

// sync samples can't start from zero
typedef struct SyncSampleRecord {
    UInt32	sampleNum;
} SyncSampleRecord;

OSErr Validate_stss_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;
	UInt32 entryCount;
	SyncSampleRecord *listP;
	UInt32 listSize;
	UInt32 i;
	
	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );

	// Get data 
	BAILIFERR( GetFileDataN32( aoe, &entryCount, offset, &offset ) );
	listSize = entryCount * sizeof(SyncSampleRecord);
	BAILIFNIL( listP = malloc(listSize), allocFailedErr );
	BAILIFERR( GetFileData( aoe, listP, offset, listSize, &offset ) );
	for ( i = 0; i < entryCount; i++ ) {
		listP[i].sampleNum = EndianU32_BtoN(listP[i].sampleNum);
	}
	
	// Print atom contents non-required fields
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);
	atomprint("entryCount=\"%ld\"\n", entryCount);
	atomprint("/>\n");
	vg.tabcnt++;
	for ( i = 0; i < entryCount; i++ ) {
		atomprintdetailed("<stssEntry sampleNum=\"%d\" />\n", listP[i].sampleNum);
		if (listP[i].sampleNum == 0) {
			errprint("You can't have a zero sample number in stss\n");
		}
	}
	--vg.tabcnt;

	// Check required field values
	FieldMustBe( flags, 0, "'stss' flags must be %d not 0x%lx" );

	// All done
	aoe->aoeflags |= kAtomValidated;

bail:
	return err;
}

//==========================================================================================

typedef struct ShadowSyncEntry {
    UInt32	shadowSyncNumber;
    UInt32	syncSampleNumber;
} ShadowSyncEntry;

OSErr Validate_stsh_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;
	UInt32 entryCount;
	ShadowSyncEntry *listP;
	UInt32 listSize;
	UInt32 i;
	
	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );

	// Get data 
	BAILIFERR( GetFileDataN32( aoe, &entryCount, offset, &offset ) );
	listSize = entryCount * sizeof(ShadowSyncEntry);
	BAILIFNIL( listP = malloc(listSize), allocFailedErr );
	BAILIFERR( GetFileData( aoe, listP, offset, listSize, &offset ) );
	for ( i = 0; i < entryCount; i++ ) {
		listP[i].shadowSyncNumber = EndianU32_BtoN(listP[i].shadowSyncNumber);
		listP[i].syncSampleNumber = EndianU32_BtoN(listP[i].syncSampleNumber);
	}
	
	// Print atom contents non-required fields
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);
	atomprint("entryCount=\"%ld\"\n", entryCount);
	atomprint("/>\n");
	vg.tabcnt++;
	for ( i = 0; i < entryCount; i++ ) {
		atomprintdetailed("<stshEntry shadowSyncNumber=\"%d\" syncSampleNumber=\"%d\" />\n", 
			listP[i].shadowSyncNumber, listP[i].syncSampleNumber );
//		if (listP[i].sampleOffset < 0) {
//			errprint("You can't have a negative offset in the ctts table\n");
//		}
	}
	--vg.tabcnt;

	// Check required field values
	FieldMustBe( flags, 0, "'stsh' flags must be %d not 0x%lx" );

	// All done
	aoe->aoeflags |= kAtomValidated;

bail:
	return err;
}

//==========================================================================================

// sync samples can't start from zero
typedef struct DegradationPriority {
    UInt16 priority; // 15-bits - top bit must be zero
} DegradationPriority;

OSErr Validate_stdp_Atom( atomOffsetEntry *aoe, void *refcon )
{
	TrackInfoRec *tir = (TrackInfoRec *)refcon;
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;
	UInt32 entryCount;
	DegradationPriority *listP;
	UInt32 listSize;
	UInt32 i;
	
	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );

	// Get data 
	//BAILIFERR( GetFileDataN32( aoe, &entryCount, offset, &offset ) );
	entryCount = tir->sampleSizeEntryCnt;
	
	if (entryCount==0) { 
		errprint("Cannot validate stdp box as it must follow sample size box to get entry count\n");
		err = badAtomErr;
		goto bail;
	}
	
	listSize = entryCount * sizeof(DegradationPriority);
	BAILIFNIL( listP = malloc(listSize), allocFailedErr );
	BAILIFERR( GetFileData( aoe, listP, offset, listSize, &offset ) );
	for ( i = 0; i < entryCount; i++ ) {
		listP[i].priority = EndianU16_BtoN(listP[i].priority);
	}
	
	// Print atom contents non-required fields
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);
	atomprint("entryCount=\"%ld\"\n", entryCount);
	atomprint("/>\n");
	vg.tabcnt++;
	for ( i = 0; i < entryCount; i++ ) {
		atomprintdetailed("<stdpEntry priority=\"%d\" />\n", listP[i].priority);
		if (listP[i].priority & (1<<15)) {
			errprint("priority top bit must be zero in stdp\n");
		}
	}
	--vg.tabcnt;

	// Check required field values
	FieldMustBe( flags, 0, "'stdp' flags must be %d not 0x%lx" );

	// All done
	aoe->aoeflags |= kAtomValidated;

bail:
	return err;
}

//==========================================================================================

OSErr Validate_sdtp_Atom( atomOffsetEntry *aoe, void *refcon )
{
	TrackInfoRec *tir = (TrackInfoRec *)refcon;
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;
	UInt32 entryCount;
	UInt8 *listP;
	UInt32 listSize;
	UInt32 i;
	
	static char* sample_depends_on[] = {
		"unk", 
		"dpnds",
		"not-dpnds",
		"res" };
	static char * sample_is_depended_on[] = { 
		"unk",
		"dpdned-on",
		"not-depnded-on",
		"res" };
	static char * sample_has_redundancy[] = {
		"unk",
		"red-cod",
		"no-red-cod",
		"res" };

	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );

	// Get data 
	//BAILIFERR( GetFileDataN32( aoe, &entryCount, offset, &offset ) );
	entryCount = tir->sampleSizeEntryCnt;
	
	if (entryCount==0) { 
		errprint("Cannot validate sdtp box as it must follow sample size box to get entry count\n");
		err = badAtomErr;
		goto bail;
	}
	
	listSize = entryCount * sizeof(UInt8);
	BAILIFNIL( listP = malloc(listSize), allocFailedErr );
	BAILIFERR( GetFileData( aoe, listP, offset, listSize, &offset ) );
	
	// Print atom contents non-required fields
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);
	atomprint("entryCount=\"%ld\"\n", entryCount);
	atomprint("/>\n");
	vg.tabcnt++;
	for ( i = 0; i < entryCount; i++ ) {
		UInt8 resvd, depends, dependedon, red;
		resvd =		 ((listP[i]) >> 6) & 3;
		depends =	 ((listP[i]) >> 4) & 3;
		dependedon = ((listP[i]) >> 2) & 3;
		red =		 ((listP[i]) >> 0) & 3;
		
		atomprintdetailed("<sdtpEntry %d=\"%s,%s,%s\" />\n", 
			i,
			sample_depends_on[depends], 
			sample_is_depended_on[dependedon],
			sample_has_redundancy[red] );
		if (resvd != 0) {
			errprint("reserved bits %d in sdtp, should be 0\n",resvd);
		}
	}
	--vg.tabcnt;

	// Check required field values
	FieldMustBe( flags, 0, "'sdtp' flags must be %d not 0x%lx" );

	// All done
	aoe->aoeflags |= kAtomValidated;

bail:
	return err;
}

//==========================================================================================

OSErr Validate_padb_Atom( atomOffsetEntry *aoe, void *refcon )
{
	TrackInfoRec *tir = (TrackInfoRec *)refcon;
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;
	UInt32 entryCount;
	UInt8 *listP;
	UInt32 listSize;
	UInt32 i;
	
	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );

	// Get data 
	BAILIFERR( GetFileDataN32( aoe, &entryCount, offset, &offset ) );

	listSize = entryCount * sizeof(UInt8);
		// 1 based array + room for one over for the 4-bit case loop
	BAILIFNIL( listP = malloc(listSize + sizeof(UInt8) + sizeof(UInt8)), allocFailedErr );

	for (i=0; i<((entryCount+1)/2); i++) {
		UInt8 thePads;
		BAILIFERR( GetFileData( aoe, &thePads, offset, 1, &offset ) );
		listP[i*2 + 1] = (thePads >> 4) & 0x7;
		listP[i*2 + 2] = thePads & 0x07;
		if ((thePads & 0x84) !=0) errprint("reserved bits must be zero in padding bits entries\n");
	}

	// Print atom contents non-required fields
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);
	atomprint("entryCount=\"%ld\"\n", entryCount);
	atomprint("/>\n");
	vg.tabcnt++;
	for ( i = 1; i <= entryCount; i++ ) {
		atomprintdetailed("<paddingBitsEntry %d=%d />\n", 
			i,
			listP[i] );
	}
	--vg.tabcnt;

	// Check required field values
	FieldMustBe( flags, 0, "'padb' flags must be %d not 0x%lx" );

	// All done
	aoe->aoeflags |= kAtomValidated;

bail:
	return err;
}

//==========================================================================================

typedef struct EditListEntryVers0Record {
    UInt32	duration;
    UInt32	mediaTime;
    Fixed		mediaRate;
} EditListEntryVers0Record;

typedef struct EditListEntryVers1Record {
    UInt64	duration;
    SInt64	mediaTime;
    Fixed		mediaRate;
} EditListEntryVers1Record;

OSErr Validate_elst_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;
	UInt32 entryCount;
	EditListEntryVers1Record *listP;
	UInt32 listSize;
	UInt32 i;
 //   Fixed mediaRate_1_0 = EndianU32_BtoN(0x10000);
	Fixed mediaRate_1_0 = 0x10000;	
	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );
        
 


	// Get data 
	BAILIFERR( GetFileDataN32( aoe, &entryCount, offset, &offset ) );
	if (entryCount == 0) {
		errprint("Edit list has an illegal entryCount of zero\n");
	}
	if (entryCount) {
    

		listSize = entryCount * sizeof(EditListEntryVers1Record);
		BAILIFNIL( listP = malloc(listSize), allocFailedErr );

		if (version == 0) {
			UInt32 list0Size;
			EditListEntryVers0Record *list0P;
		
			list0Size = entryCount * sizeof(EditListEntryVers0Record);
			BAILIFNIL( list0P = malloc(list0Size), allocFailedErr );
			BAILIFERR( GetFileData( aoe, list0P, offset, list0Size, &offset ) );
			for ( i = 0; i < entryCount; i++ ) {
				listP[i].duration = EndianU32_BtoN(list0P[i].duration);
				listP[i].mediaTime = EndianS32_BtoN(list0P[i].mediaTime);
				listP[i].mediaRate = EndianU32_BtoN(list0P[i].mediaRate);
			}
		} else if (version == 1) {
			BAILIFERR( GetFileData( aoe, listP, offset, listSize, &offset ) );
			for ( i = 0; i < entryCount; i++ ) {
				listP[i].duration = EndianU64_BtoN(listP[i].duration);
				listP[i].mediaTime = EndianS64_BtoN(listP[i].mediaTime);
				listP[i].mediaRate = EndianU32_BtoN(listP[i].mediaRate);
			}
		} else {
			errprint("Edit list is version other than 0 or 1\n");
			err = badAtomErr;
			goto bail;
		}
	}
	
	// Print atom contents non-required fields
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);
	atomprint("entryCount=\"%ld\"\n", entryCount);
	atomprint("/>\n");
	vg.tabcnt++;
	for ( i = 0; i < entryCount; i++ ) {
		atomprint("<elstEntry duration=\"%s\"", int64todstr(listP[i].duration));
		atomprintnotab(" mediaTime=\"%s\"", int64todstr(listP[i].mediaTime));
		if ((listP[i].mediaTime < 0) && (listP[i].mediaTime != -1))
			errprint("Edit list:  the only allowed negative value for media time is -1\n");
		atomprintnotab(" mediaRate=\"%s\" />\n", fixed32str(listP[i].mediaRate));

		switch (vg.filetype) {
			default:		// ISO family
				if ((listP[i].mediaRate != mediaRate_1_0) && (listP[i].mediaRate != 0))	
					errprint("Edit list: media rate can only be 0 or 1, not 0x%0X\n", listP[i].mediaRate);
		}

//		if (listP[i].mediaRate & 0x0000ffff) {
//			errprint("mpeg4 in their infinite wisdom has declared -1(Fixed) is 0xffff0000 NOT 0xffffffff\n");
//		}
	}
	--vg.tabcnt;


	// Check required field values
	
	// All done
	aoe->aoeflags |= kAtomValidated;

bail:
	return err;

}


//==========================================================================================

void check_track_ID( UInt32 theID )
{
	MovieInfoRec	*mir = vg.mir;
	UInt32 i;
	
	if (theID==0) {
		errprint("Track ID %d in track reference atoms cannot be zero\n",theID);
		return;
	}
	
	for (i=0; i<mir->numTIRs; ++i) {
			if ((mir->tirList[i].trackID) == theID) return;
	}		
	errprint("Track ID %d in track reference atoms references a non-existent track\n",theID);
}


OSErr Validate_tref_type_Atom( atomOffsetEntry *aoe, void *refcon, OSType trefType, UInt32 *firstRefTrackID )
{
#pragma unused(refcon)
	OSErr err = noErr;
	UInt64 offset;
	UInt32 entryCount;
	UInt32 *listP;
	UInt32 listSize;
	UInt32 i;

	// Get data 
	offset = aoe->offset + aoe->atomStartSize;
	listSize = (UInt32)(aoe->size - aoe->atomStartSize);
	entryCount = listSize / sizeof(UInt32);
	BAILIFNIL( listP = malloc(listSize), allocFailedErr );
	BAILIFERR( GetFileData( aoe, listP, offset, listSize, &offset ) );
	for ( i = 0; i < entryCount; i++ ) {
		listP[i] = EndianU32_BtoN(listP[i]);
		check_track_ID( listP[i] );
	}

	// Print atom contents non-required fields
	atomprintnotab(">\n");
	//vg.tabcnt++;
	for ( i = 0; i < entryCount; i++ ) {
		atomprint("<tref%sEntry trackID=\"%ld\" />\n", ostypetostr(trefType), listP[i]);
	}
	//--vg.tabcnt;

	if ((entryCount > 0) && (firstRefTrackID != NULL)) {
		*firstRefTrackID = listP[0];
	}

	// All done
	aoe->aoeflags |= kAtomValidated;

bail:
	return err;
}

OSErr Validate_tref_hint_Atom( atomOffsetEntry *aoe, void *refcon )
{
	OSErr			err = noErr;
	UInt32			refTrackID = 0;
	TrackInfoRec	*tir = (TrackInfoRec*)refcon;

	err = Validate_tref_type_Atom( aoe, refcon, 'hint', &refTrackID );
	if (err == noErr) {
		tir->hintRefTrackID = refTrackID;
	}
	return err;
}

OSErr Validate_tref_dpnd_Atom( atomOffsetEntry *aoe, void *refcon )
{
	return Validate_tref_type_Atom( aoe, refcon, 'dpnd', NULL );
}

OSErr Validate_tref_ipir_Atom( atomOffsetEntry *aoe, void *refcon )
{
	return Validate_tref_type_Atom( aoe, refcon, 'ipir', NULL );
}

OSErr Validate_tref_mpod_Atom( atomOffsetEntry *aoe, void *refcon )
{
	return Validate_tref_type_Atom( aoe, refcon, 'mpod', NULL);
}

OSErr Validate_tref_sync_Atom( atomOffsetEntry *aoe, void *refcon )
{
	return Validate_tref_type_Atom( aoe, refcon, 'sync', NULL );
}


//==========================================================================================


OSErr Validate_stsd_Atom( atomOffsetEntry *aoe, void *refcon )
{
	TrackInfoRec *tir = (TrackInfoRec *)refcon;
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;
	UInt32 entryCount;
	SampleDescriptionPtr *sampleDescriptionPtrArray;
	UInt32 *validatedSampleDescriptionRefCons;
	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );

	// Get data 
	BAILIFERR( GetFileDataN32( aoe, &entryCount, offset, &offset ) );
		// 1 based table
	BAILIFNULL( sampleDescriptionPtrArray = calloc((entryCount + 1), sizeof(SampleDescriptionPtr)), allocFailedErr );
	BAILIFNULL( validatedSampleDescriptionRefCons = calloc((entryCount + 1), sizeof(UInt32)), allocFailedErr );
	
	// Print atom contents non-required fields
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);
	atomprint("entryCount=\"%ld\"\n", entryCount);
	atomprint(">\n"); //vg.tabcnt++; 

	// Check required field values
	FieldMustBe( flags, 0, "'dref' flags must be %d not 0x%lx" );

	//need to validate sample descriptions
	tir->sampleDescriptionCnt = entryCount;
	tir->sampleDescriptions = sampleDescriptionPtrArray;
	tir->validatedSampleDescriptionRefCons = validatedSampleDescriptionRefCons;
	{
		UInt64 minOffset, maxOffset;
		atomOffsetEntry *entry;
		long cnt;
		atomOffsetEntry *list;
		int i;
		
		minOffset = offset;
		maxOffset = aoe->maxOffset;
		
		BAILIFERR( FindAtomOffsets( aoe, minOffset, maxOffset, &cnt, &list ) );
		
		if (cnt != 1) {
			errprint( "MPEG-4 only allows 1 sample description\n" );
			err = badAtomErr;
		}
		
		for (i = 0; i < cnt; i++) {
			entry = &list[i];
			

			{  // stash the sample description
				SampleDescriptionPtr sdp;
				sdp = malloc( entry->size );
				err = GetFileData( entry, (void*)sdp, entry->offset, entry->size, nil );
				sampleDescriptionPtrArray[i+1] = sdp;
			}
			atomprint("<%s_sampledescription\n",ostypetostr(tir->mediaType)); vg.tabcnt++;
			tir->currentSampleDescriptionIndex = i+1;
			
			switch( tir->mediaType ) {
				case 'vide':
					err = Validate_vide_SD_Entry( entry, refcon );
					break;

				case 'soun':
					err = Validate_soun_SD_Entry( entry, refcon );
					break;
					
				case 'hint':
					err = Validate_hint_SD_Entry( entry, refcon );
					break;
					
				case 'sdsm':
					err = Validate_mp4_SD_Entry( entry, refcon, Validate_sdsm_ES_Bitstream, "sdsm_ES" );
					break;
					
				case 'odsm':
					err = Validate_mp4_SD_Entry( entry, refcon, Validate_odsm_ES_Bitstream, "odsm_ES" );
					break;
					
					
				default:
					// why does MP4 say it must be an MpegSampleEntry?
					//   So by default you can't have any other media type!!!
					err = Validate_mp4_SD_Entry( entry, refcon, Validate_mp4s_ES_Bitstream, "mp4_ES" );
					break;
			}
			--vg.tabcnt; atomprint("</%s_sampledescription>\n",ostypetostr(tir->mediaType));

		}

	}
	
	
	// All done
	aoe->aoeflags |= kAtomValidated;
bail:
	return err;


}


//==========================================================================================

OSErr Validate_vide_SD_Entry( atomOffsetEntry *aoe, void *refcon )
{
	TrackInfoRec *tir = (TrackInfoRec *)refcon;
	OSErr err = noErr;
	UInt64 offset;
	SampleDescriptionHead sdh;
	VideoSampleDescriptionInfo vsdi;
	
	offset = aoe->offset;

	// Get data 
	BAILIFERR( GetFileData( aoe, &sdh, offset, sizeof(sdh), &offset ) );
	EndianSampleDescriptionHead_BtoN( &sdh );
	
	// Note the sample description for both 'mp4v' and 's263' are the same in all these fields
	BAILIFERR( GetFileData( aoe, &vsdi, offset, fieldOffset(VideoSampleDescriptionInfo,extensions) /* sizeof(vsdi) */, &offset ) );
	vsdi.version = EndianS16_BtoN(vsdi.version);
	vsdi.revisionLevel = EndianS16_BtoN(vsdi.revisionLevel);
	vsdi.vendor = EndianU32_BtoN(vsdi.vendor);
	vsdi.temporalQuality = EndianU32_BtoN(vsdi.temporalQuality);
	vsdi.spatialQuality = EndianU32_BtoN(vsdi.spatialQuality);
	vsdi.width = EndianS16_BtoN(vsdi.width);
	vsdi.height = EndianS16_BtoN(vsdi.height);
	
	tir->sampleDescWidth = vsdi.width; tir->sampleDescHeight = vsdi.height;
	if ((tir->trackWidth>>16) != vsdi.width) {
		warnprint("WARNING: Sample description width %d not the same as track width %s\n",vsdi.width,fixedU32str(tir->trackWidth));
	}
	if ((tir->trackHeight>>16) != vsdi.height) {
		warnprint("WARNING: Sample description height %d not the same as track height %s\n",vsdi.height,fixedU32str(tir->trackHeight));
	}
	if ((vsdi.width==0) || (vsdi.height==0)) {
		errprint("Visual Sample description height (%d) or width (%d) zero\n",vsdi.height,vsdi.width);
	}
	
	vsdi.hRes = EndianU32_BtoN(vsdi.hRes);
	vsdi.vRes = EndianU32_BtoN(vsdi.vRes);
	vsdi.dataSize = EndianU32_BtoN(vsdi.dataSize);
	vsdi.frameCount = EndianS16_BtoN(vsdi.frameCount);
	// vsdi.name
	vsdi.depth = EndianS16_BtoN(vsdi.depth);
	vsdi.clutID = EndianS16_BtoN(vsdi.clutID);
	// Print atom contents non-required fields
	atomprint("sdType=\"%s\"\n", ostypetostr(sdh.sdType));
	atomprint("dataRefIndex=\"%ld\"\n", sdh.dataRefIndex);
	// atomprint(">\n"); //vg.tabcnt++; 

	// Check required field values
	atomprint("version=\"%hd\"\n", vsdi.version);
	atomprint("revisionLevel =\"%hd\"\n", vsdi.revisionLevel);
	atomprint("vendor =\"%s\"\n", ostypetostr(vsdi.vendor));
	atomprint("temporalQuality =\"%ld\"\n", vsdi.temporalQuality);
	atomprint("spatialQuality =\"%ld\"\n", vsdi.spatialQuality);
	atomprint("width =\"%hd\"\n", vsdi.width);
	atomprint("height =\"%hd\"\n", vsdi.height);
	atomprint("hRes =\"%s\"\n", fixedU32str(vsdi.hRes));
	atomprint("vRes =\"%s\"\n", fixedU32str(vsdi.vRes));
	atomprint("dataSize =\"%ld\"\n", vsdi.dataSize);
	atomprint("frameCount =\"%hd\"\n", vsdi.frameCount);
	atomprint("name =\"%s\"\n", vsdi.name);
	atomprint("depth =\"%hd\"\n", vsdi.depth);
	atomprint("clutID =\"%hd\"\n", vsdi.clutID);
		FieldMustBeOneOf3( sdh.sdType, OSType, "SampleDescription sdType must be 'mp4v' or 'avc1'", ('mp4v', 'avc1', 'encv') );
		
	FieldMustBe( sdh.resvd1, 0, "SampleDescription resvd1 must be %d not %d" );
	FieldMustBe( sdh.resvdA, 0, "SampleDescription resvd1 must be %d not %d" );


	FieldMustBe( vsdi.version, 0, "ImageDescription version must be %d not %d" );
	FieldMustBe( vsdi.revisionLevel, 0, "ImageDescription revisionLevel must be %d not %d" );
	FieldMustBe( vsdi.vendor, 0, "ImageDescription vendor must be %d not 0x%lx" );
	FieldMustBe( vsdi.temporalQuality, 0, "ImageDescription temporalQuality must be %d not %d" );
	FieldMustBe( vsdi.spatialQuality, 0, "ImageDescription spatialQuality must be %d not %d" );
	if( vg.majorBrand == brandtype_mp41 ){
		if (vsdi.width  != 320) warnprint("Warning: You signal brand MP4v1 and there ImageDescription width must be 320 not %d\n", vsdi.width );
		if (vsdi.height != 240) warnprint("Warning: You signal brand MP4v1 and there ImageDescription height must be 240 not %d\n", vsdi.height );
	}

	FieldMustBe( vsdi.hRes, 72L<<16, "ImageDescription hRes must be 72.0 (0x%lx) not 0x%lx" );
	FieldMustBe( vsdi.vRes, 72L<<16, "ImageDescription vRes must be 72.0 (0x%lx) not 0x%lx" );
	FieldMustBe( vsdi.dataSize, 0, "ImageDescription dataSize must be %d not %d" );
	FieldMustBe( vsdi.frameCount, 1, "ImageDescription frameCount must be %d not %d" );
	// should check the whole string
	FieldMustBe( vsdi.name[0], 0, "ImageDescription name must be ''" );
	FieldMustBe( vsdi.depth, 24, "ImageDescription depth must be %d not %d" );
	FieldMustBe( vsdi.clutID, -1, "ImageDescription clutID must be %d not %d" );

		// Now we have the Sample Extensions
		{
			UInt64 minOffset, maxOffset;
			atomOffsetEntry *entry;
			long cnt;
			atomOffsetEntry *list;
			int i;
			int is_protected = 0;
			
			minOffset = offset;
			maxOffset = aoe->offset + aoe->size;
			
			is_protected = ( sdh.sdType == 'drmi' ) || (( (sdh.sdType & 0xFFFFFF00) | ' ') == 'enc ' );
			
			BAILIFERR( FindAtomOffsets( aoe, minOffset, maxOffset, &cnt, &list ) );
			if ((cnt != 1) && (sdh.sdType == 'mp4v')) {
				errprint( "MPEG-4 only allows 1 sample description extension\n" );
				err = badAtomErr;
			}
			
			for (i = 0; i < cnt; i++) {
				entry = &list[i];
				
				if (entry->type == 'esds') {
					BAILIFERR( Validate_ESDAtom( entry, refcon, Validate_vide_ES_Bitstream, "vide_ES" ) );
				}
				else if ( entry->type == 'uuid' ) 
				{
					// Process 'uuid' atoms
					atomprint("<uuid"); vg.tabcnt++;
					BAILIFERR( Validate_uuid_Atom( entry, refcon ) );
					--vg.tabcnt; atomprint("</uuid>\n");
				}
				else if ( entry->type == 'sinf' ) 
				{
					// Process 'sinf' atoms
					atomprint("<sinf"); vg.tabcnt++;
					BAILIFERR( Validate_sinf_Atom( entry, refcon, kTypeAtomFlagMustHaveOne ) );
					--vg.tabcnt; atomprint("</sinf>\n");
				}				
				else if ( entry->type == 'colr' ) 
				{
					// Process 'colr' atoms
					atomprint("<colr"); vg.tabcnt++;
					BAILIFERR( Validate_colr_Atom( entry, refcon ) );
					--vg.tabcnt; atomprint("</colr>\n");
				}				

				else if (( sdh.sdType == 'avc1' ) || is_protected) 
				{
					if (entry->type == 'avcC') {
						BAILIFERR( Validate_avcC_Atom( entry, refcon, "avcC" ) );
					}
					else if (entry->type == 'svcC') {
						BAILIFERR( Validate_avcC_Atom( entry, refcon, "svcC" ) );
					}
					else if (entry->type == 'mvcC') {
						BAILIFERR( Validate_avcC_Atom( entry, refcon, "mvcC" ) );
					}
					else if ( entry->type == 'btrt'){
						BAILIFERR( Validate_btrt_Atom( entry, refcon, "btrt" ) );
					}
					else if ( entry->type == 'm4ds' ){
						BAILIFERR( Validate_m4ds_Atom( entry, refcon, "m4ds" ) );
					}

					else { 
						err = badAtomErr;
						warnprint("Warning: Unknown atom found \"%s\": video sample descriptions would not normally contain this\n",ostypetostr(entry->type));
						//goto bail;
					}
				}
				else { 
					err = badAtomErr;
					warnprint("Warning: Unknown atom found \"%s\": video sample descriptions would not normally contain this\n",ostypetostr(entry->type));
					// goto bail;
				}
				
			}
		}
	
	// All done
	aoe->aoeflags |= kAtomValidated;

bail:
	return err;
}

//==========================================================================================

typedef struct SoundSampleDescriptionInfo {
	SInt16		version;                    /* which version is this data */
	SInt16		revisionLevel;              /* what version of that codec did this */
	UInt32		vendor;                     /* whose  codec compressed this data */
	SInt16		numChannels;                /* number of channels of sound */
	SInt16		sampleSize;                 /* number of bits per sample */
	SInt16		compressionID;              /* unused. set to zero. */
	SInt16		packetSize;                 /* unused. set to zero. */
	UInt32		sampleRate;					/*еее UnsignedFixed еее*/ /* sample rate sound is captured at */
} SoundSampleDescriptionInfo;

OSErr Validate_soun_SD_Entry( atomOffsetEntry *aoe, void *refcon )
{
	TrackInfoRec *tir = (TrackInfoRec *)refcon;
	OSErr err = noErr;
	Boolean fileTypeKnown = false;
	UInt64 offset;
	SampleDescriptionHead sdh;
	SoundSampleDescriptionInfo ssdi;
	UInt16 sampleratelo, sampleratehi;
	
	offset = aoe->offset;

	// Get data 
	BAILIFERR( GetFileData( aoe, &sdh, offset, sizeof(sdh), &offset ) );
	EndianSampleDescriptionHead_BtoN( &sdh );
	BAILIFERR( GetFileData( aoe, &ssdi, offset, sizeof(ssdi), &offset ) );
	ssdi.version = EndianS16_BtoN(ssdi.version);
	ssdi.revisionLevel = EndianS16_BtoN(ssdi.revisionLevel);
	ssdi.vendor = EndianU32_BtoN(ssdi.vendor);
	ssdi.numChannels = EndianS16_BtoN(ssdi.numChannels);
	ssdi.sampleSize = EndianS16_BtoN(ssdi.sampleSize);
	ssdi.compressionID = EndianS16_BtoN(ssdi.compressionID);
	ssdi.packetSize = EndianS16_BtoN(ssdi.packetSize);
	ssdi.sampleRate = EndianU32_BtoN(ssdi.sampleRate);
	
	// Print atom contents non-required fields
	atomprint("sdType=\"%s\"\n", ostypetostr(sdh.sdType));
	atomprint("dataRefIndex=\"%ld\"\n", sdh.dataRefIndex);
	atomprint("sampleRate=\"%s\"\n", fixedU32str(ssdi.sampleRate));
	
	sampleratelo = (ssdi.sampleRate) & 0xFFFF;
	sampleratehi = (ssdi.sampleRate >> 16) & 0xFFFF;
	
	if (sampleratehi != tir->mediaTimeScale) {
		warnprint("WARNING: Track timescale %d not equal to the (integer part of) the Sample entry sample rate %d.%d\n", 
				tir->mediaTimeScale, sampleratehi, sampleratelo);
		}
	atomprint(">\n"); //vg.tabcnt++; 

	// Check required field values
	FieldMustBeOneOf2( sdh.sdType, OSType, "SampleDescription sdType must be 'mp4a' or 'enca' ", ( 'mp4a', 'enca' ) );
	
	if( (sdh.sdType != 'mp4a') && (sdh.sdType != 'enca') && !fileTypeKnown ){	
			warnprint("WARNING: Don't know about this sound descriptor type \"%s\"\n", 
				ostypetostr(sdh.sdType));
			// goto bail;
	}  
		
	FieldMustBe( sdh.resvd1, 0, "SampleDescription resvd1 must be %d not 0x%lx" );
	FieldMustBe( sdh.resvdA, 0, "SampleDescription resvd1 must be %d not 0x%lx" );
	FieldMustBe( ssdi.version, 0, "SoundDescription version must be %d not %d" );
	FieldMustBe( ssdi.revisionLevel, 0, "SoundDescription revisionLevel must be %d not %d" );
	FieldMustBe( ssdi.vendor, 0, "SoundDescription vendor must be %d not 0x%lx" );
	FieldMustBe( ssdi.numChannels, 2, "SoundDescription numChannels must be %d not %d" );
	FieldMustBe( ssdi.sampleSize, 16, "SoundDescription sampleSize must be %d not %d" );
	FieldMustBe( ssdi.compressionID, 0, "SoundDescription compressionID must be %d not %d" );
	FieldMustBe( ssdi.packetSize, 0, "SoundDescription packetSize must be %d not %d" );
	// sample rate must be time-scale of track << 16 for mp4


	FieldMustBe( ssdi.sampleRate & 0x0000ffff, 0, "SoundDescription sampleRate's low long must be %d not 0x%lx" );
	
	// Now we have the Sample Extensions

	{
		UInt64 minOffset;
		UInt64 maxOffset;
		atomOffsetEntry *entry;
		long cnt;
		atomOffsetEntry *list;
		int i;
		
		minOffset = offset;
		maxOffset = aoe->offset + aoe->size;
		
		BAILIFERR( FindAtomOffsets( aoe, minOffset, maxOffset, &cnt, &list ) );
		
		if ((cnt != 1) && (sdh.sdType == 'mp4v')) {
			errprint( "MPEG-4 only allows 1 sample description extension\n" );
			err = badAtomErr;
		}
		
		for (i = 0; i < cnt; i++) {
			entry = &list[i];
			
			if (entry->type == 'esds') { 
				BAILIFERR( Validate_ESDAtom( entry, refcon, Validate_soun_ES_Bitstream, "soun_ES" ) ); 
			}
			
			else if ( entry->type == 'sinf' ) 
			{
				// Process 'sinf' atoms
				atomprint("<sinf"); vg.tabcnt++;
				BAILIFERR( Validate_sinf_Atom( entry, refcon, kTypeAtomFlagMustHaveOne ) );
				--vg.tabcnt; atomprint("</sinf>\n");
			}				
			
			else warnprint("Warning: Unknown atom found \"%s\": audio sample descriptions would not normally contain this\n",ostypetostr(entry->type));
			
		}
	}

	// All done
	aoe->aoeflags |= kAtomValidated;

bail:
	return err;
}


//==========================================================================================

OSErr Validate_hint_SD_Entry( atomOffsetEntry *aoe, void *refcon )
{

	TrackInfoRec *tir = (TrackInfoRec *)refcon;
	OSErr err = noErr;
	UInt64 offset;
	SampleDescriptionHead sdh;
	
	offset = aoe->offset;

	// Get data 
	BAILIFERR( GetFileData( aoe, &sdh, offset, sizeof(sdh), &offset ) );
	EndianSampleDescriptionHead_BtoN( &sdh );
//еее╩how to cope with hint data
	
	// Print atom contents non-required fields
	atomprint("sdType=\"%s\"\n", ostypetostr(sdh.sdType));
	atomprint("dataRefIndex=\"%ld\"\n", sdh.dataRefIndex);
	atomprint(">\n"); //vg.tabcnt++; 

	// Check required field values
	FieldMustBe( sdh.resvd1, 0, "SampleDescription resvd1 must be %d not 0x%lx" );
	FieldMustBe( sdh.resvdA, 0, "SampleDescription resvd1 must be %d not 0x%lx" );
	
//еее hint data
	
	// All done
	aoe->aoeflags |= kAtomValidated;

bail:
	return err;
}

//==========================================================================================

OSErr Validate_mp4_SD_Entry( atomOffsetEntry *aoe, void *refcon, ValidateBitstreamProcPtr validateBitstreamProc, char *esname )
{
	TrackInfoRec *tir = (TrackInfoRec *)refcon;
	OSErr err = noErr;
	UInt64 offset;
	SampleDescriptionHead sdh;
	
	offset = aoe->offset;

	// Get data 
	BAILIFERR( GetFileData( aoe, &sdh, offset, sizeof(sdh), &offset ) );
	EndianSampleDescriptionHead_BtoN( &sdh );
//еее╩how to cope with hint data
	
	// Print atom contents non-required fields
	atomprint("sdType=\"%s\"\n", ostypetostr(sdh.sdType));
	atomprint("dataRefIndex=\"%ld\"\n", sdh.dataRefIndex);
	atomprint(">\n"); //vg.tabcnt++; 



	FieldMustBe( sdh.sdType, 'mp4s', "SampleDescription sdType must be 'mp4s'" );
	FieldMustBe( sdh.resvd1, 0, "SampleDescription resvd1 must be %d not 0x%lx" );
	FieldMustBe( sdh.resvdA, 0, "SampleDescription resvd1 must be %d not 0x%lx" );
	
	// Now we have the Sample Extensions
	{
		UInt64 minOffset, maxOffset;
		atomOffsetEntry *entry;
		long cnt;
		atomOffsetEntry *list;
		int i;
		
		minOffset = offset;
		maxOffset = aoe->offset + aoe->size;
		
		BAILIFERR( FindAtomOffsets( aoe, minOffset, maxOffset, &cnt, &list ) );
		
		if (cnt != 1) {

				errprint( "MPEG-4 only allows 1 ESD\n" );
			err = badAtomErr;
		}
		
		for (i = 0; i < cnt; i++) {
			entry = &list[i];
			
			
			BAILIFERR( Validate_ESDAtom( entry, refcon, validateBitstreamProc, esname) );
			
		}
	}
	
	
	// All done
	aoe->aoeflags |= kAtomValidated;

bail:
	return err;
}






//==========================================================================================

OSErr Validate_ESDAtom( atomOffsetEntry *aoe, void *refcon, ValidateBitstreamProcPtr validateBitstreamProc, char *esname )
{
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;
	Ptr esDataP = nil;
	unsigned long esSize;
	BitBuffer bb;
	
	atomprint("<ESD"); vg.tabcnt++;
	
	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );
	

		FieldMustBe( flags, 0, "'ESDAtom' flags must be %d not 0x%lx" );
		FieldMustBe( version, 0, "ESDAtom version must be %d not 0x%2x" );

	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags); 
	atomprint(">\n");
	
	// Get the ObjectDescriptor
	atomprint("<%s>", esname); vg.tabcnt++;
	BAILIFERR( GetFileBitStreamDataToEndOfAtom( aoe, &esDataP, &esSize, offset, &offset ) );
	
	BitBuffer_Init(&bb, (void *)esDataP, esSize);

	BAILIFERR( CallValidateBitstreamProc( validateBitstreamProc, &bb, refcon ) );
	
	if (NumBytesLeft(&bb) > 1) {
		err = tooMuchDataErr;
	}
		
	--vg.tabcnt; atomprint("</%s>\n", esname);
	
	// All done
	aoe->aoeflags |= kAtomValidated;
	--vg.tabcnt; atomprint("</ESD>\n");
	
bail:
	if (esDataP)
		free(esDataP);

	return err;
}



OSErr Validate_uuid_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	UInt64 offset;
	AtomSizeType start;
	UInt32 residual;
	Boolean temp;
	//char	tempStr[100];
	
	// atomprint("<uuid "); vg.tabcnt++;
	residual = aoe->size - sizeof( AtomSizeType ) - sizeof( uuidType );
	
	atomprintnotab("\tuuid=\"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\" more_data_length=\"%d\" %s\n", 
		aoe->uuid[0],  aoe->uuid[1],  aoe->uuid[2],  aoe->uuid[3], 
		aoe->uuid[4],  aoe->uuid[5],  aoe->uuid[6],  aoe->uuid[7], 
		aoe->uuid[8],  aoe->uuid[9],  aoe->uuid[10], aoe->uuid[11], 
		aoe->uuid[12], aoe->uuid[13], aoe->uuid[14], aoe->uuid[15], 
		residual, ">" ); // (residual > 0 ? ">" : "/>")
	
	offset = aoe->offset + 8 + 16;
	
	temp = vg.printsample; vg.tabcnt++;
	vg.printsample = true;

	while (residual>0) {
		UInt32 to_read;
		char buff[16];
		
		to_read = (residual > 16 ? 16 : residual);
		BAILIFERR( GetFileData( aoe, &(buff[0]), offset, to_read, &offset ) );
		sampleprinthexandasciidata( &(buff[0]), to_read );
		residual -= to_read;
	}
	--vg.tabcnt; vg.printsample = false;
	residual = aoe->size - sizeof( AtomSizeType ) - sizeof( uuidType );
	// if (residual > 0) atomprint("</uuid>\n");  --vg.tabcnt; 
	
	// All done
	aoe->aoeflags |= kAtomValidated;
	
bail:
	return err;
}

OSErr Validate_colr_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	UInt64 offset;
	ColrInfo colrHeader;
	static char* primaries[] = {
		"Reserved", "BT.709", "Unspecified", "Reserved","BT.470-2 System M",
		"EBU Tech. 3213 (was BT.470-2 System B,G)", "SMPTE 170M", "SMPTE 240M", "Linear/Film", 
		"Log 100:1", "Log 316.22777:1"};
	static char* matrices[] = {
		"Reserved", "BT.709", "Unspecified", "Reserved","FCC",
		"BT.470-2 System B,G", "ITU-R BT.601-4 (SMPTE 170M)", "SMPTE 240M" };
	char* prim;
	char* func;
	char* matr;
			
	// Get version/flags
	offset = aoe->offset;
	BAILIFERR( GetFileData( aoe, &colrHeader.start.atomSize, offset, sizeof( AtomSizeType ), &offset ) );
	colrHeader.start.atomSize = EndianU32_BtoN( colrHeader.start.atomSize );
	colrHeader.start.atomType = EndianU32_BtoN( colrHeader.start.atomType );

	BAILIFERR( GetFileData( aoe, &colrHeader.colrtype, offset, colrHeader.start.atomSize - sizeof( AtomSizeType ), &offset ) );
	colrHeader.colrtype = EndianU32_BtoN( colrHeader.colrtype );
	
	if ((colrHeader.colrtype == 'nclc') && ( 18 == colrHeader.start.atomSize )) {
		colrHeader.primaries = EndianU16_BtoN( colrHeader.primaries );
		if (colrHeader.primaries < 11) prim = primaries[colrHeader.primaries]; else prim = "unknown";
		
		colrHeader.function  = EndianU16_BtoN( colrHeader.function );
		if (colrHeader.function < 11) func = primaries[colrHeader.function]; else func = "unknown";

		colrHeader.matrix    = EndianU16_BtoN( colrHeader.matrix );
		if (colrHeader.matrix < 8) matr = matrices[colrHeader.matrix]; else matr = "unknown";
		atomprintnotab("\tavg Colr/nclc Primaries=\"%d\" (%s), function=\"%d\" (%s), matrix=\"%d\" (%s)\n", 
			colrHeader.primaries, prim,
			colrHeader.function, func,
			colrHeader.matrix, matr);
			
		atomprint(">\n"); 
	}
	else errprint( "colr atom size or type not as expected; size %d, should be %d; or type %s not nclc\n", 
		colrHeader.start.atomSize, 18, ostypetostr(colrHeader.colrtype) );
			
	// All done
	aoe->aoeflags |= kAtomValidated;
	
bail:
	return err;
}

OSErr Validate_avcC_Atom( atomOffsetEntry *aoe, void *refcon, char *esname )
{
#pragma unused(refcon)
	OSErr err = noErr;
	UInt64 offset;
	AvcConfigInfo avcHeader;
	void* bsDataP;
	BitBuffer bb;
	
	atomprint("<%s", esname); vg.tabcnt++;
	
	// Get version/flags
	offset = aoe->offset;
	BAILIFERR( GetFileData( aoe, &avcHeader.start.atomSize, offset, sizeof( AtomSizeType ), &offset ) );
	avcHeader.start.atomSize = EndianU32_BtoN( avcHeader.start.atomSize );
	avcHeader.start.atomType = EndianU32_BtoN( avcHeader.start.atomType );
	

	BAILIFNIL( bsDataP = calloc(avcHeader.start.atomSize - 8 + bitParsingSlop, 1), allocFailedErr );
	BAILIFERR( GetFileData( aoe, bsDataP, offset, avcHeader.start.atomSize - 8, &offset ) );
	BitBuffer_Init(&bb, (void *)bsDataP, avcHeader.start.atomSize - 8);

	BAILIFERR( Validate_AVCConfigRecord( &bb, refcon ) );		
	--vg.tabcnt; atomprint("/>\n");


	free( bsDataP );

	// All done
	aoe->aoeflags |= kAtomValidated;
	
bail:
	return err;
}

OSErr Validate_btrt_Atom( atomOffsetEntry *aoe, void *refcon, char *esName )
{
#pragma unused(refcon)
	OSErr err = noErr;
	UInt64 offset;
	AvcBtrtInfo bitrHeader;
	//char	tempStr[100];
	
	atomprint("<btrt"); vg.tabcnt++;
	
		
	// Get version/flags
	offset = aoe->offset;
	BAILIFERR( GetFileData( aoe, &bitrHeader.start.atomSize, offset, sizeof( AtomSizeType ), &offset ) );
	bitrHeader.start.atomSize = EndianU32_BtoN( bitrHeader.start.atomSize );
	bitrHeader.start.atomType = EndianU32_BtoN( bitrHeader.start.atomType );

	BAILIFERR( GetFileData( aoe, &bitrHeader.buffersizeDB, offset, bitrHeader.start.atomSize - sizeof( AtomSizeType ), &offset ) );
	bitrHeader.buffersizeDB = EndianU32_BtoN( bitrHeader.buffersizeDB );
	bitrHeader.maxBitrate   = EndianU32_BtoN( bitrHeader.maxBitrate );
	bitrHeader.avgBitrate   = EndianU32_BtoN( bitrHeader.avgBitrate );
	
	atomprintnotab("\tBuffzersizeDB=\"%d\"  max Bitrate=\"%d\" avg Bitrate=\"%d\"\n", 
		bitrHeader.buffersizeDB, bitrHeader.maxBitrate, bitrHeader.avgBitrate );
	atomprint(">\n"); 
	
	if( sizeof( AvcBtrtInfo ) != bitrHeader.start.atomSize ){
		err = badAtomSize;
		errprint( "atom size for 'btrt' atom (%d) != sizeof( AvcBtrtInfo )(%d) \n", bitrHeader.start.atomSize, sizeof( AvcBtrtInfo ) );
		goto bail;
	}
		
	--vg.tabcnt; atomprint(">\n");

	// All done
	aoe->aoeflags |= kAtomValidated;
	
bail:
	return err;
}

OSErr Validate_m4ds_Atom( atomOffsetEntry *aoe, void *refcon, char *esName )
{
	OSErr err = noErr;
	UInt64 offset;
	Ptr esDataP = nil;
	unsigned long esSize;
	BitBuffer bb;
	
	atomprint("<m4ds>\n"); vg.tabcnt++;
	offset = aoe->offset + aoe->atomStartSize;
	
	// Get the Descriptors
	BAILIFERR( GetFileBitStreamDataToEndOfAtom( aoe, &esDataP, &esSize, offset, &offset ) );
	
	BitBuffer_Init(&bb, (void *)esDataP, esSize);

	while (NumBytesLeft(&bb) >= 1) {
		BAILIFERR( Validate_Random_Descriptor(  &bb, "Descriptor" ) );
	}
	
	// All done
	aoe->aoeflags |= kAtomValidated;
	--vg.tabcnt; atomprint("</m4ds>\n");
	
bail:
	if (esDataP)
		free(esDataP);

	return err;
}

//==========================================================================================

OSErr Validate_cprt_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;
	Ptr esDataP = nil;
	UInt16 language;
	char *noticeP = nil;
	UInt16 stringSize;
	int textIsUTF16 = false;		// otherwise, UTF-8
	int utf16TextIsLittleEndian = false;
	
	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );
	FieldMustBe( version, 0, "cprt version must be %d not %d" );
	FieldMustBe( flags, 0, "cprt flags must be %d not 0x%lx" );
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);
	
	// Get data
	BAILIFERR( GetFileDataN16( aoe, &language, offset, &offset ) );
	FieldMustBe( (language & 0x8000), 0, "cprt language's high bit must be 0" );
	atomprint("language=\"%s\"\n", langtodstr(language));
	if (language==0) warnprint("WARNING: Copyright language code of 0 not strictly legit -- 'und' preferred\n");


#if 1
	// This may be a UTF-16 string so we can't just grab a C string
	
	stringSize = aoe->maxOffset - offset;
	noticeP = (char*) calloc(stringSize, 1);
	
	BAILIFERR( GetFileData( aoe, noticeP, offset, stringSize, &offset ) );

	// check string type
	if (stringSize > 2) {
		UInt16 possibleBOM = EndianU16_BtoN(*(UInt16*)noticeP);
		
		if (possibleBOM == 0x0feff) {			// big endian
			textIsUTF16 = true;
		}
		else if (possibleBOM == 0x0fffe) {		// little endian
			textIsUTF16 = true;
			utf16TextIsLittleEndian = true;
		}
	}
	
	// if text is UTF-16, we will generate ASCII text for output
	if (textIsUTF16) {
		char * utf8noticeP = nil;
		char * pASCII = nil;
		UInt16 * pUTF16 = nil;
		int numChars = (stringSize - 2)/2;
		
		if (numChars == 0) { // no actual text
			errprint("UTF-16 text has BOM but no terminator\n");
		}
		else {
			int ix;
			
			// ее clf -- The right solution is probably to generate "\uNNNN" for Unicode characters not in the range 0-0x7f. That
			// will require the array be 5 times as large in the worst case.
			utf8noticeP = calloc(numChars, 1);
			pASCII= utf8noticeP;
			
			pUTF16 = (UInt16*) (noticeP + 2);
			
			for (ix=0; ix < numChars-1; ix++, pUTF16++) {
				UInt16 utf16Char = utf16TextIsLittleEndian ? EndianU16_LtoN(*pUTF16) : EndianU16_BtoN(*pUTF16);
				
				*pASCII	= (utf16Char & 0xff80) ? ((char) '\?') : (char)(utf16Char & 0x7f);
				
				pASCII++;
			}
			
			free(noticeP);
			noticeP = utf8noticeP;
		}
	}
#else
	BAILIFERR( GetFileCString( aoe, &noticeP, offset, aoe->maxOffset - offset, &offset ) );
#endif
	atomprint("notice=\"%s\"\n", noticeP);

	atomprint("/>\n"); 

	// All done
	aoe->aoeflags |= kAtomValidated;

bail:
	if (esDataP)
		free(esDataP);

	return err;
}

//==========================================================================================

OSErr Validate_loci_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	UInt32 version;
	UInt32 flags;
	UInt64 offset;
	Ptr esDataP = nil;
	UInt16 language;
	char *noticeP = nil;
	UInt16 stringSize;
	int textIsUTF16 = false;		// otherwise, UTF-8
	int utf16TextIsLittleEndian = false;
	SInt32 lngi, lati, alti;
	UInt8 role;
	
	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );
	FieldMustBe( version, 0, "loci version must be %d not %d" );
	FieldMustBe( flags, 0, "loci flags must be %d not 0x%lx" );
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);
	
	// Get data
	BAILIFERR( GetFileDataN16( aoe, &language, offset, &offset ) );
	FieldMustBe( (language & 0x8000), 0, "loci language's high bit must be 0" );
	atomprint("language=\"%s\"\n", langtodstr(language));
	if (language==0) warnprint("WARNING: Location language code of 0 not strictly legit -- 'und' preferred\n");
	
	stringSize = aoe->maxOffset - offset;
	noticeP = (char*) calloc(stringSize, 1);
		
	BAILIFERR( GetFileUTFString( aoe, &noticeP, offset, aoe->maxOffset - offset, &offset ) );
	atomprint("Name=\"%s\"\n", noticeP);
	
	BAILIFERR( GetFileData( aoe, &role,  offset, 1, &offset ) );
	BAILIFERR( GetFileData( aoe, &lngi, offset, 4, &offset ) ); lngi = EndianS32_BtoN(lngi);
	BAILIFERR( GetFileData( aoe, &lati, offset, 4, &offset ) ); lati = EndianS32_BtoN(lati);
	BAILIFERR( GetFileData( aoe, &alti, offset, 4, &offset ) ); alti = EndianS32_BtoN(alti);
	
	atomprint("role=\"%d\"\n", role );
	
	atomprint("longitude=\"%d.%d\"\n", lngi >> 16, ((UInt32) lngi) && 0xFFFF );
	atomprint("latitude=\"%d.%d\"\n",  lati >> 16, ((UInt32) lati) && 0xFFFF );
	atomprint("altitude=\"%d.%d\"\n",  alti >> 16, ((UInt32) alti) && 0xFFFF );
	

	BAILIFERR( GetFileUTFString( aoe, &noticeP, offset, aoe->maxOffset - offset, &offset ) );
	atomprint("Body=\"%s\"\n", noticeP);

	BAILIFERR( GetFileUTFString( aoe, &noticeP, offset, aoe->maxOffset - offset, &offset ) );
	atomprint("Notes=\"%s\"\n", noticeP);

	atomprint("/>\n"); 
	
	// All done
	aoe->aoeflags |= kAtomValidated;
	
bail:
	if (esDataP)
		free(esDataP);
	
	return err;
}

//==========================================================================================

//==========================================================================================

OSErr Validate_frma_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	UInt64 offset;
	AtomSizeType ahdr;
	UInt32 format;
	//char	tempStr[100];
	
	// Get version/flags
	offset = aoe->offset;
	BAILIFERR( GetFileData( aoe, &ahdr, offset, sizeof( AtomSizeType ), &offset ) );
	ahdr.atomSize = EndianU32_BtoN( ahdr.atomSize );
	ahdr.atomType = EndianU32_BtoN( ahdr.atomType );


	BAILIFERR( GetFileData( aoe, &format, offset, sizeof( UInt32 ), &offset ) );
	format = EndianU32_BtoN( format );
	
	atomprintnotab("\toriginal_format=\"%s\"\n", ostypetostr(format) );
	atomprint(">\n"); 
	
	if( ahdr.atomSize != (sizeof(UInt32) + sizeof(AtomSizeType)) ){
		err = badAtomSize;
		errprint( "wrong atom size for 'frma' atom (%d) should be %d \n", ahdr.atomSize, (sizeof(UInt32) + sizeof(AtomSizeType)) );
		goto bail;
	}
		
	// All done
	aoe->aoeflags |= kAtomValidated;
	
bail:
	return err;
}

OSErr Validate_schm_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	UInt64 offset;
	AtomSizeType ahdr;
	UInt32 scheme, s_version, vers, flags;
	//char	tempStr[100];
	char *locationP = nil;
	
	// Get version/flags
	offset = aoe->offset;
	BAILIFERR( GetFileData( aoe, &ahdr, offset, sizeof( AtomStartRecord ), &offset ) );
	ahdr.atomSize = EndianU32_BtoN( ahdr.atomSize );
	ahdr.atomType = EndianU32_BtoN( ahdr.atomType );

	BAILIFERR( GetFullAtomVersionFlags( aoe, &vers, &flags, &offset));

	BAILIFERR( GetFileData( aoe, &scheme,    offset, sizeof( UInt32 ), &offset ) );
	BAILIFERR( GetFileData( aoe, &s_version, offset, sizeof( UInt32 ), &offset ) );
	scheme    = EndianU32_BtoN( scheme );
	s_version = EndianU32_BtoN( s_version );
	
	atomprintnotab("\tscheme=\"%s\" version=\"%d\"\n", ostypetostr(scheme), s_version );
	// Get data 
	if (flags & 1) {
		BAILIFERR( GetFileCString( aoe, &locationP, offset, aoe->maxOffset - offset, &offset ) );
		atomprint("location=\"%s\"\n", locationP);
	}
	
	atomprint(">\n"); 
	
	// All done
	aoe->aoeflags |= kAtomValidated;
	
bail:
	return err;
}

OSErr Validate_schi_Atom( atomOffsetEntry *aoe, void *refcon )
{
	OSErr err = noErr;
	long cnt;
	atomOffsetEntry *list;
	long i;
	OSErr atomerr = noErr;
	atomOffsetEntry *entry;
	UInt64 minOffset, maxOffset;
	TrackInfoRec *tir = (TrackInfoRec *)refcon;
	
	atomprintnotab(">\n"); 
	
	minOffset = aoe->offset + aoe->atomStartSize;
	maxOffset = aoe->offset + aoe->size - aoe->atomStartSize;
	
	BAILIFERR( FindAtomOffsets( aoe, minOffset, maxOffset, &cnt, &list ) );
	atomprint(" comment=\"%d contained atoms\" >\n",cnt);
	//
	for (i = 0; i < cnt; i++) {
		entry = &list[i];

		if (entry->aoeflags & kAtomValidated) continue;

		switch (entry->type) {
			default:
				warnprint("WARNING: unknown schi atom '%s' length %ld\n",ostypetostr(entry->type), entry->size);
				break;
		}
		
		if (!err) err = atomerr;
	}
	
	// All done
	aoe->aoeflags |= kAtomValidated;
	
bail:
	return err;
}

//==========================================================================================

OSErr Validate_xml_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	UInt64 offset;
	AtomSizeType ahdr;
	UInt32 vers, flags;
	//char	tempStr[100];
	char *xmlP = nil;
	
	// Get version/flags
	offset = aoe->offset;
	BAILIFERR( GetFileData( aoe, &ahdr, offset, sizeof( AtomStartRecord ), &offset ) );
	ahdr.atomSize = EndianU32_BtoN( ahdr.atomSize );
	ahdr.atomType = EndianU32_BtoN( ahdr.atomType );

	BAILIFERR( GetFullAtomVersionFlags( aoe, &vers, &flags, &offset));
	
	// Get data 
	if (ahdr.atomType == 'xml ') {
		BAILIFERR( GetFileCString( aoe, &xmlP, offset, aoe->maxOffset - offset, &offset ) );
		atomprint("XML=\"%s\"\n", xmlP);
	}
	else atomprintnotab("\t..contains %d bytes\n", ahdr.atomSize - 8 );

	atomprint(">\n"); 
	
	// All done
	aoe->aoeflags |= kAtomValidated;
	
bail:
	return err;
}

OSErr Validate_iloc_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	UInt64 offset;
	AtomSizeType ahdr;
	UInt32 i, temp, vers, flags, offset_size, length_size, base_offset_size;
	UInt16 item_count;
	UInt8 temp8;
	//char	tempStr[100];
	
	// Get version/flags
	offset = aoe->offset;
	BAILIFERR( GetFileData( aoe, &ahdr, offset, sizeof( AtomStartRecord ), &offset ) );
	ahdr.atomSize = EndianU32_BtoN( ahdr.atomSize );
	ahdr.atomType = EndianU32_BtoN( ahdr.atomType );

	BAILIFERR( GetFullAtomVersionFlags( aoe, &vers, &flags, &offset));

	BAILIFERR( GetFileData( aoe, &temp8,    offset, 1, &offset ) );
	offset_size = (temp8 >> 4) & 0x0F;
	length_size = temp8 & 0x0F;
	BAILIFERR( GetFileData( aoe, &temp8,    offset, 1, &offset ) );
	base_offset_size = (temp8 >> 4) & 0x0F;

	BAILIFERR( GetFileData( aoe, &item_count, offset, 2, &offset ) );
	item_count    = EndianU16_BtoN( item_count );

	atomprintnotab("\toffset_size=\"%d\" length_size=\"%d\" base_offset_size=\"%d\" item_count=\"%d\">\n", 
			offset_size, length_size, base_offset_size, item_count);

	vg.tabcnt++;
	for (i=0; i<item_count; i++) {
		UInt16 item_id, dref_idx, ext_count;
		UInt64 base_offset;
		UInt32 j;
		BAILIFERR( GetFileData( aoe, &item_id, offset, 2, &offset ) );
		item_id    = EndianU16_BtoN( item_id );
		BAILIFERR( GetFileData( aoe, &dref_idx, offset, 2, &offset ) );
		dref_idx    = EndianU16_BtoN( dref_idx );
		switch (base_offset_size) {
			case 0: base_offset = 0; break;
			case 4: 
				BAILIFERR( GetFileData( aoe, &temp, offset, 4, &offset ) );
				base_offset    = EndianU32_BtoN( temp );
				break;
			case 8: 
				BAILIFERR( GetFileData( aoe, &base_offset, offset, 8, &offset ) );
				base_offset    = EndianU64_BtoN( base_offset );
				break;
			default:
				errprint("You can't have a base offset size of %d in iloc\n", base_offset_size);
		}
		BAILIFERR( GetFileData( aoe, &ext_count, offset, 2, &offset ) );
		ext_count    = EndianU16_BtoN( ext_count );
		atomprint("<item item_id=\"%d\" dref_idx=\"%d\" base_offset=\"%s\" ext_count=\"%d\">\n", 
			item_id, dref_idx, int64todstr(base_offset), ext_count);
		vg.tabcnt++;
		for (j=0; j<ext_count; j++) {
			UInt64 e_offset, e_length;
			char temp1[100];
			char temp2[100];
			switch (offset_size) {
				case 0: e_offset = 0; break;
				case 4: 
					BAILIFERR( GetFileData( aoe, &temp, offset, 4, &offset ) );
					e_offset    = EndianU32_BtoN( temp );
					break;
				case 8: 
					BAILIFERR( GetFileData( aoe, &e_offset, offset, 8, &offset ) );
					e_offset    = EndianU64_BtoN( e_offset );
					break;
				default:
					errprint("You can't have an offset size of %d in iloc\n", offset_size);
			}
			switch (length_size) {
				case 0: e_length = 0; break;
				case 4: 
					BAILIFERR( GetFileData( aoe, &temp, offset, 4, &offset ) );
					e_length    = EndianU32_BtoN( temp );
					break;
				case 8: 
					BAILIFERR( GetFileData( aoe, &e_length, offset, 8, &offset ) );
					e_length    = EndianU64_BtoN( e_length );
					break;
				default:
					errprint("You can't have a length size of %d in iloc\n", length_size);
			}
			atomprint("<extent extent_offset=\"%s\" extent_length=\"%s\"\\>\n", int64todstr_r(e_offset, temp1), int64todstr_r(e_length,temp2));
		}
		--vg.tabcnt;
		atomprint("<\\item>\n");
	}
	--vg.tabcnt;
		
	// All done
	aoe->aoeflags |= kAtomValidated;
	
bail:
	return err;
}

OSErr Validate_pitm_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	UInt64 offset;
	AtomSizeType ahdr;
	UInt16 item_id;
	UInt32 vers, flags;
	//char	tempStr[100];
	
	// Get version/flags
	offset = aoe->offset;
	BAILIFERR( GetFileData( aoe, &ahdr, offset, sizeof( AtomSizeType ), &offset ) );
	ahdr.atomSize = EndianU32_BtoN( ahdr.atomSize );
	ahdr.atomType = EndianU32_BtoN( ahdr.atomType );

	BAILIFERR( GetFullAtomVersionFlags( aoe, &vers, &flags, &offset));

	BAILIFERR( GetFileData( aoe, &item_id, offset, sizeof( UInt16 ), &offset ) );
	item_id = EndianU16_BtoN( item_id );
	
	atomprintnotab("\tprimary_item_ID=\"%d\"\n", item_id );
	atomprint(">\n"); 
	
	if( ahdr.atomSize != (6 + sizeof(AtomSizeType)) ){
		err = badAtomSize;
		errprint( "wrong atom size for 'pitm' atom (%d) should be %d \n", ahdr.atomSize, (6 + sizeof(AtomSizeType)) );
		goto bail;
	}
		
	// All done
	aoe->aoeflags |= kAtomValidated;
	
bail:
	return err;
}

OSErr Validate_ipro_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	UInt64 offset;
	AtomSizeType ahdr;
	UInt16 prot_count;
	UInt32 vers, flags;
	//char	tempStr[100];
	
	// Get version/flags
	offset = aoe->offset;
	BAILIFERR( GetFileData( aoe, &ahdr, offset, sizeof( AtomSizeType ), &offset ) );
	ahdr.atomSize = EndianU32_BtoN( ahdr.atomSize );
	ahdr.atomType = EndianU32_BtoN( ahdr.atomType );

	BAILIFERR( GetFullAtomVersionFlags( aoe, &vers, &flags, &offset));

	BAILIFERR( GetFileData( aoe, &prot_count, offset, sizeof( UInt16 ), &offset ) );
	prot_count = EndianU16_BtoN( prot_count );
	
	atomprintnotab("\tprot_count=\"%d\"\n", prot_count );
	vg.tabcnt++;
	{
		UInt64 minOffset;
		UInt64 maxOffset;
		atomOffsetEntry *entry;
		long cnt;
		atomOffsetEntry *list;
		int i;
		
		minOffset = offset;
		maxOffset = aoe->offset + aoe->size;
		
		BAILIFERR( FindAtomOffsets( aoe, minOffset, maxOffset, &cnt, &list ) );
		
		if (cnt != prot_count) errprint("Found %d atoms but expected %d\n", cnt, prot_count);
		
		for (i = 0; i < cnt; i++) {
			entry = &list[i];
			
			if ( entry->type == 'sinf' ) 
			{
				// Process 'sinf' atoms
				atomprint("<sinf"); vg.tabcnt++;
				BAILIFERR( Validate_sinf_Atom( entry, refcon, 0 ) );
				--vg.tabcnt; atomprint("</sinf>\n");
			}				
			
			else warnprint("Warning: Unknown atom found \"%s\": ipro atoms would not normally contain this\n",ostypetostr(entry->type));
			
		}
	}
	--vg.tabcnt;
	atomprint(">\n"); 
	
	// All done
	aoe->aoeflags |= kAtomValidated;
	
bail:
	return err;
}

OSErr Validate_infe_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	UInt64 offset;
	AtomSizeType ahdr;
	UInt32 scheme, s_version, vers, flags;
	//char	tempStr[100];
	char *nameP = nil;
	char *typeP = nil;
	char *encodP = nil;
	UInt16 item_id, prot_idx;
	
	// Get version/flags
	offset = aoe->offset;
	BAILIFERR( GetFileData( aoe, &ahdr, offset, sizeof( AtomStartRecord ), &offset ) );
	ahdr.atomSize = EndianU32_BtoN( ahdr.atomSize );
	ahdr.atomType = EndianU32_BtoN( ahdr.atomType );

	BAILIFERR( GetFullAtomVersionFlags( aoe, &vers, &flags, &offset));

	BAILIFERR( GetFileData( aoe, &item_id,    offset, sizeof( UInt16 ), &offset ) );
	BAILIFERR( GetFileData( aoe, &prot_idx, offset, sizeof( UInt16 ), &offset ) );
	item_id  = EndianU16_BtoN( item_id );
	prot_idx = EndianU16_BtoN( prot_idx );
	
	atomprintnotab("\t item_id=\"%d\" protection_index=\"%d\"\n", item_id, prot_idx );
	BAILIFERR( GetFileCString( aoe, &nameP, offset, aoe->maxOffset - offset, &offset ) );
	atomprint("item_name=\"%s\"\n", nameP);
	BAILIFERR( GetFileCString( aoe, &typeP, offset, aoe->maxOffset - offset, &offset ) );
	atomprint("content_type=\"%s\"\n", typeP);
	BAILIFERR( GetFileCString( aoe, &encodP, offset, aoe->maxOffset - offset, &offset ) );
	atomprint("content_encoding=\"%s\"\n", encodP);
	
	atomprint(">\n"); 
	
	// All done
	aoe->aoeflags |= kAtomValidated;
	
bail:
	return err;
}

OSErr Validate_iinf_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	UInt64 offset;
	AtomSizeType ahdr;
	UInt16 inf_count;
	UInt32 vers, flags;
	//char	tempStr[100];
	
	// Get version/flags
	offset = aoe->offset;
	BAILIFERR( GetFileData( aoe, &ahdr, offset, sizeof( AtomSizeType ), &offset ) );
	ahdr.atomSize = EndianU32_BtoN( ahdr.atomSize );
	ahdr.atomType = EndianU32_BtoN( ahdr.atomType );

	BAILIFERR( GetFullAtomVersionFlags( aoe, &vers, &flags, &offset));

	BAILIFERR( GetFileData( aoe, &inf_count, offset, sizeof( UInt16 ), &offset ) );
	inf_count = EndianU16_BtoN( inf_count );
	
	atomprintnotab("\tinf_count=\"%d\"\n", inf_count );
	vg.tabcnt++;
	{
		UInt64 minOffset;
		UInt64 maxOffset;
		atomOffsetEntry *entry;
		long cnt;
		atomOffsetEntry *list;
		int i;
		
		minOffset = offset;
		maxOffset = aoe->offset + aoe->size;
		
		BAILIFERR( FindAtomOffsets( aoe, minOffset, maxOffset, &cnt, &list ) );
		
		if (cnt != inf_count) errprint("Found %d atoms but expected %d\n", cnt, inf_count);
		
		for (i = 0; i < cnt; i++) {
			entry = &list[i];
			
			if ( entry->type == 'infe' ) 
			{
				// Process 'infe' atoms
				atomprint("<infe"); vg.tabcnt++;
				BAILIFERR( Validate_infe_Atom( entry, refcon ) );
				--vg.tabcnt; atomprint("</infe>\n");
			}				
			
			else warnprint("Warning: Unknown atom found \"%s\": iinf atoms would not normally contain this\n",ostypetostr(entry->type));
			
		}
	}
	--vg.tabcnt;
	atomprint(">\n"); 
	
	// All done
	aoe->aoeflags |= kAtomValidated;
	
bail:
	return err;
}

