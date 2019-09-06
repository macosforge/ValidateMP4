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

	// for use with ostypetostr_r() and int64todstr_r() for example;
    // when you're using one of these routines more than once in the same print statement
	char   tempStr1[32];
	char   tempStr2[32];
	char   tempStr3[32];
	char   tempStr4[32];
	char   tempStr5[32];
	char   tempStr6[32];
	char   tempStr7[32];
	char   tempStr8[32];
	char   tempStr9[32];
	char   tempStr10[32];


//==========================================================================================

int FindAtomOffsets( atomOffsetEntry *aoe, UInt64 minOffset, UInt64 maxOffset, 
			long *atomCountOut, atomOffsetEntry **atomOffsetsOut )
{
	int err = noErr;
	long cnt = 0;
	atomOffsetEntry *atomOffsets = nil;
	long max = 20;
	startAtomType startAtom;
	UInt64 largeSize;
	uuidType uuid;
	UInt64 curOffset = minOffset;
	atomOffsetEntry zeroAtom = {0};
	long minAtomSize;
	
	BAILIFNULL( atomOffsets = calloc( max, sizeof(atomOffsetEntry)), allocFailedErr );
	
	while (curOffset< maxOffset) {
		memset(&atomOffsets[cnt], 0, sizeof(atomOffsetEntry));	// clear out entry
		atomOffsets[cnt].offset = curOffset;
		BAILIFERR( GetFileDataN32( aoe, &startAtom.size, curOffset, &curOffset ) );
		BAILIFERR( GetFileDataN32( aoe, &startAtom.type, curOffset, &curOffset ) );
		minAtomSize = sizeof(startAtom);
		atomOffsets[cnt].size = startAtom.size;
		atomOffsets[cnt].type = startAtom.type;
		if (startAtom.size == 1) {
			BAILIFERR( GetFileDataN64( aoe, &largeSize, curOffset, &curOffset ) );
			atomOffsets[cnt].size = largeSize;
			minAtomSize += sizeof(largeSize);
			
		}
		if (startAtom.type == 'uuid') {
			BAILIFERR( GetFileData( aoe, &uuid, curOffset, sizeof(uuid), &curOffset ) );
			//atomOffsets[cnt].uuid = uuid;
			memcpy(&atomOffsets[cnt].uuid, &uuid, sizeof(uuid));
			minAtomSize += sizeof(uuid);
		}
		
		atomOffsets[cnt].atomStartSize = minAtomSize;
		atomOffsets[cnt].maxOffset = atomOffsets[cnt].offset + atomOffsets[cnt].size;
		
		if (atomOffsets[cnt].size == 0) {
			// we go to the end
			atomOffsets[cnt].size = maxOffset - atomOffsets[cnt].offset;
			break;
		}
		
		BAILIF( (atomOffsets[cnt].size < minAtomSize), badAtomSize );
		
		curOffset = atomOffsets[cnt].offset + atomOffsets[cnt].size;
		cnt++;
		if (cnt >= max) {
			max += 20;
			atomOffsets = realloc(atomOffsets, max * sizeof(atomOffsetEntry));
		}
	}

bail:
	if (err) {
		cnt = 0;
		if (atomOffsets) 
			free(atomOffsets);
		atomOffsets = nil;
	}
	*atomCountOut = cnt;
	*atomOffsetsOut = atomOffsets;
	return err;
}

//==========================================================================================

OSErr ValidateFileAtoms( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	long cnt;
	atomOffsetEntry *list;
	long i;
	OSErr atomerr = noErr;
	long moovCnt = 0;
	atomOffsetEntry *entry;
	UInt64 minOffset, maxOffset;
	
	minOffset = aoe->offset + aoe->atomStartSize;
	maxOffset = aoe->offset + aoe->size - aoe->atomStartSize;
	
	BAILIFERR( FindAtomOffsets( aoe, minOffset, maxOffset, &cnt, &list ) );
	
	// Process 'ftyp' atom
	
	atomerr = ValidateAtomOfType( 'ftyp', kTypeAtomFlagMustHaveOne | kTypeAtomFlagCanHaveAtMostOne | kTypeAtomFlagMustBeFirst, 
		Validate_ftyp_Atom, cnt, list, nil );
	if (!err) err = atomerr;
	
	// Process 'moov' atoms
	vg.mir = NULL; 
	atomerr = ValidateAtomOfType( 'moov', kTypeAtomFlagMustHaveOne | kTypeAtomFlagCanHaveAtMostOne, 
		Validate_moov_Atom, cnt, list, nil );
	if (!err) err = atomerr;
	
	// Process 'meta' atoms
	atomerr = ValidateAtomOfType( 'meta', kTypeAtomFlagCanHaveAtMostOne, 
		Validate_meta_Atom, cnt, list, nil );
	if (!err) err = atomerr;
	
	//
	for (i = 0; i < cnt; i++) {
		entry = &list[i];

		switch (entry->type) {
			case 'mdat':

			case 'skip':
			case 'free':
				break;
				
			
			case 'uuid':
					atomerr = ValidateAtomOfType( 'uuid', 0, 
						Validate_uuid_Atom, cnt, list, nil );
					if (!err) err = atomerr;
					break;

			default:
				if (!(entry->aoeflags & kAtomValidated)) 
					warnprint("WARNING: unknown file atom '%s'\n",ostypetostr(entry->type));
				break;
		}
		
		if (!err) err = atomerr;
	}
	
	aoe->aoeflags |= kAtomValidated;
bail:
	if ( vg.mir != NULL) {
		dispose_mir(vg.mir);
	}

	return err;
}

//==========================================================================================

OSErr Validate_dinf_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	long cnt;
	atomOffsetEntry *list;
	long i;
	OSErr atomerr = noErr;
	long mvhdCnt = 0;
	long trakCnt = 0;
	long iodsCnt = 0;
	atomOffsetEntry *entry;
	UInt64 minOffset, maxOffset;
	
	atomprintnotab(">\n"); 
	
	minOffset = aoe->offset + aoe->atomStartSize;
	maxOffset = aoe->offset + aoe->size - aoe->atomStartSize;
	
	BAILIFERR( FindAtomOffsets( aoe, minOffset, maxOffset, &cnt, &list ) );
	
	// Process 'dref' atoms
	atomerr = ValidateAtomOfType( 'dref', kTypeAtomFlagMustHaveOne | kTypeAtomFlagCanHaveAtMostOne, 
		Validate_dref_Atom, cnt, list, nil );
	if (!err) err = atomerr;

	//
	for (i = 0; i < cnt; i++) {
		entry = &list[i];

		if (entry->aoeflags & kAtomValidated) continue;

		switch (entry->type) {
			default:
				warnprint("WARNING: unknown data information atom '%s'\n",ostypetostr(entry->type));
				break;
		}
		
		if (!err) err = atomerr;
	}
	
	aoe->aoeflags |= kAtomValidated;
bail:
	return err;
}
//==========================================================================================
OSErr Validate_edts_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	long cnt;
	atomOffsetEntry *list;
	long i;
	OSErr atomerr = noErr;
	long mvhdCnt = 0;
	long trakCnt = 0;
	long iodsCnt = 0;
	atomOffsetEntry *entry;
	UInt64 minOffset, maxOffset;
	
	atomprintnotab(">\n"); 
	
	minOffset = aoe->offset + aoe->atomStartSize;
	maxOffset = aoe->offset + aoe->size - aoe->atomStartSize;
	
	BAILIFERR( FindAtomOffsets( aoe, minOffset, maxOffset, &cnt, &list ) );
	
	// Process 'elst' atoms
	atomerr = ValidateAtomOfType( 'elst', kTypeAtomFlagCanHaveAtMostOne, 
		Validate_elst_Atom, cnt, list, nil );
	if (!err) err = atomerr;

	//
	for (i = 0; i < cnt; i++) {
		entry = &list[i];

		if (entry->aoeflags & kAtomValidated) continue;

		switch (entry->type) {
			default:
				warnprint("WARNING: unknown edit list atom '%s'\n",ostypetostr(entry->type));
				break;
		}
		
		if (!err) err = atomerr;
	}
	
	aoe->aoeflags |= kAtomValidated;
bail:
	return err;
}
//==========================================================================================

OSErr Validate_minf_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	long cnt;
	atomOffsetEntry *list;
	long i;
	OSErr atomerr = noErr;
	long mvhdCnt = 0;
	long trakCnt = 0;
	long iodsCnt = 0;
	atomOffsetEntry *entry;
	UInt64 minOffset, maxOffset;
	TrackInfoRec *tir = (TrackInfoRec *)refcon;
	
	atomprintnotab(">\n"); 
	
	minOffset = aoe->offset + aoe->atomStartSize;
	maxOffset = aoe->offset + aoe->size - aoe->atomStartSize;
	
	BAILIFERR( FindAtomOffsets( aoe, minOffset, maxOffset, &cnt, &list ) );
	
	// deal with the different header atoms
	switch (tir->mediaType) {
		case 'vide':
			// Process 'vmhd' atoms
			atomerr = ValidateAtomOfType( 'vmhd', kTypeAtomFlagMustHaveOne | kTypeAtomFlagCanHaveAtMostOne, 
				Validate_vmhd_Atom, cnt, list, nil );
			if (!err) err = atomerr;
			break;
		
		case 'soun':
			// Process 'smhd' atoms
			atomerr = ValidateAtomOfType( 'smhd', kTypeAtomFlagMustHaveOne | kTypeAtomFlagCanHaveAtMostOne, 
				Validate_smhd_Atom, cnt, list, nil );
			if (!err) err = atomerr;
			break;
		
		case 'hint':
			// Process 'hmhd' atoms
			atomerr = ValidateAtomOfType( 'hmhd',kTypeAtomFlagMustHaveOne | kTypeAtomFlagCanHaveAtMostOne, 
				Validate_hmhd_Atom, cnt, list, nil );
			if (!err) err = atomerr;
			break;
		
		case 'odsm':
		case 'sdsm':
			// Process 'nmhd' atoms
			atomerr = ValidateAtomOfType( 'nmhd', kTypeAtomFlagMustHaveOne | kTypeAtomFlagCanHaveAtMostOne, 
				Validate_nmhd_Atom, cnt, list, nil );
			if (!err) err = atomerr;
			break;
		default:
			warnprint("WARNING: unknown media type '%s'\n",ostypetostr(tir->mediaType));
	}


	// Process 'dinf' atoms
	atomerr = ValidateAtomOfType( 'dinf', kTypeAtomFlagMustHaveOne | kTypeAtomFlagCanHaveAtMostOne, 
		Validate_dinf_Atom, cnt, list, nil );
	if (!err) err = atomerr;

	// Process 'stbl' atoms
	atomerr = ValidateAtomOfType( 'stbl', kTypeAtomFlagMustHaveOne | kTypeAtomFlagCanHaveAtMostOne, 
		Validate_stbl_Atom, cnt, list, tir );
	if (!err) err = atomerr;

	//
	for (i = 0; i < cnt; i++) {
		entry = &list[i];

		if (entry->aoeflags & kAtomValidated) continue;

		switch (entry->type) {
			case 'odhd':
			case 'crhd':
			case 'sdhd':
			case 'm7hd':
			case 'ochd':
			case 'iphd':
			case 'mjhd':
				errprint("'%s' media type is reserved but not currently used\n", ostypetostr(entry->type));				
				atomprint("<%s *****>\n",ostypetostr(entry->type));
				atomprint("</%s>\n",ostypetostr(entry->type));
				break;
				
			default:
				warnprint("WARNING: unknown/unexpected atom '%s'\n",ostypetostr(entry->type));
				atomprint("<%s *****>\n",ostypetostr(entry->type));
				atomprint("</%s>\n",ostypetostr(entry->type));
				break;
		}
		
		if (!err) err = atomerr;
	}
	
	aoe->aoeflags |= kAtomValidated;
bail:
	return err;
}

//==========================================================================================

OSErr Validate_mdia_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	long cnt;
	atomOffsetEntry *list;
	long i;
	OSErr atomerr = noErr;
	long mvhdCnt = 0;
	long trakCnt = 0;
	long iodsCnt = 0;
	atomOffsetEntry *entry;
	UInt64 minOffset, maxOffset;
	TrackInfoRec *tir = (TrackInfoRec *)refcon;
	
	atomprintnotab(">\n"); 
	
	minOffset = aoe->offset + aoe->atomStartSize;
	maxOffset = aoe->offset + aoe->size - aoe->atomStartSize;
	
	BAILIFERR( FindAtomOffsets( aoe, minOffset, maxOffset, &cnt, &list ) );
	
	// Process 'mdhd' atoms
	atomerr = ValidateAtomOfType( 'mdhd', kTypeAtomFlagMustHaveOne | kTypeAtomFlagCanHaveAtMostOne, 
		Validate_mdhd_Atom, cnt, list, tir );
	if (!err) err = atomerr;

	// Process 'hdlr' atoms
	atomerr = ValidateAtomOfType( 'hdlr', kTypeAtomFlagMustHaveOne | kTypeAtomFlagCanHaveAtMostOne, 
		Validate_mdia_hdlr_Atom, cnt, list, tir );
	if (!err) err = atomerr;

	// Process 'minf' atoms
	atomerr = ValidateAtomOfType( 'minf', kTypeAtomFlagMustHaveOne | kTypeAtomFlagCanHaveAtMostOne, 
		Validate_minf_Atom, cnt, list, tir );
	if (!err) err = atomerr;
	
	// Process 'uuid' atoms
	atomerr = ValidateAtomOfType( 'uuid', 0, 
		Validate_uuid_Atom, cnt, list, nil );
	if (!err) err = atomerr;

	//
	for (i = 0; i < cnt; i++) {
		entry = &list[i];

		if (entry->aoeflags & kAtomValidated) continue;

		switch (entry->type) {
			default:
				warnprint("WARNING: unknown media atom '%s'\n",ostypetostr(entry->type));
				break;
		}
		
		if (!err) err = atomerr;
	}
	
	aoe->aoeflags |= kAtomValidated;
bail:
	return err;
}

//==========================================================================================

OSErr Get_trak_Type( atomOffsetEntry *aoe, TrackInfoRec *tir )
{
	OSErr err = noErr;
	long cnt;
	atomOffsetEntry *list;
	long i;
	OSErr atomerr = noErr;
	atomOffsetEntry *entry;
	UInt64 minOffset, maxOffset;

	long entrycnt;
	atomOffsetEntry *entrylist;
	atomOffsetEntry *entryentry;
	long	j;
	
	minOffset = aoe->offset + aoe->atomStartSize;
	maxOffset = aoe->offset + aoe->size - aoe->atomStartSize;
	
	BAILIFERR( FindAtomOffsets( aoe, minOffset, maxOffset, &cnt, &list ) );
	
	for (i = 0; i < cnt; i++) {
		entry = &list[i];
		if (entry->type == 'mdia') {
			minOffset = entry->offset + entry->atomStartSize;
			maxOffset = entry->offset + entry->size - entry->atomStartSize;
			BAILIFERR( FindAtomOffsets( entry, minOffset, maxOffset, &entrycnt, &entrylist ) );
			for (j=0; j<entrycnt; ++j) {
				entryentry = &entrylist[j];
				if (entryentry->type == 'hdlr') {
					Get_mdia_hdlr_mediaType(entryentry, tir);
					break;
				}
			}
			break;
		}
	}

bail:
	return err;
}

//==========================================================================================

OSErr Validate_trak_Atom( atomOffsetEntry *aoe, void *refcon )
{
	OSErr err = noErr;
	long cnt;
	atomOffsetEntry *list;
	long i;
	OSErr atomerr = noErr;
	long mvhdCnt = 0;
	long trakCnt = 0;
	long iodsCnt = 0;
	atomOffsetEntry *entry;
	UInt64 minOffset, maxOffset;
	TrackInfoRec	*tir = (TrackInfoRec*)refcon;
	
	atomprintnotab(">\n"); 
			
	minOffset = aoe->offset + aoe->atomStartSize;
	maxOffset = aoe->offset + aoe->size - aoe->atomStartSize;
	
	BAILIFERR( FindAtomOffsets( aoe, minOffset, maxOffset, &cnt, &list ) );
	
	// Process 'tkhd' atoms
	atomerr = ValidateAtomOfType( 'tkhd', kTypeAtomFlagMustHaveOne | kTypeAtomFlagCanHaveAtMostOne, 
		Validate_tkhd_Atom, cnt, list, tir );
	if (!err) err = atomerr;

	// Process 'tref' atoms
	atomerr = ValidateAtomOfType( 'tref', kTypeAtomFlagCanHaveAtMostOne, 
		Validate_tref_Atom, cnt, list, tir );
	if (!err) err = atomerr;

	// Process 'edts' atoms
	atomerr = ValidateAtomOfType( 'edts', kTypeAtomFlagCanHaveAtMostOne, 
		Validate_edts_Atom, cnt, list, tir );
	if (!err) err = atomerr;

	// Process 'mdia' atoms
	atomerr = ValidateAtomOfType( 'mdia', kTypeAtomFlagMustHaveOne | kTypeAtomFlagCanHaveAtMostOne, 
		Validate_mdia_Atom, cnt, list, tir );
	if (!err) err = atomerr;

	// Process 'udta' atoms
	atomerr = ValidateAtomOfType( 'udta', 0, 
		Validate_udta_Atom, cnt, list, tir );
	if (!err) err = atomerr;

	// Process 'uuid' atoms
	atomerr = ValidateAtomOfType( 'uuid', 0, 
		Validate_uuid_Atom, cnt, list, tir );
	if (!err) err = atomerr;

	// Process 'meta' atoms
	atomerr = ValidateAtomOfType( 'meta', 0, 
		Validate_meta_Atom, cnt, list, tir );
	if (!err) err = atomerr;

	//
	for (i = 0; i < cnt; i++) {
		entry = &list[i];

		if (entry->aoeflags & kAtomValidated) continue;

		switch (entry->type) {
			default:
				warnprint("WARNING: unknown trak atom '%s'\n",ostypetostr(entry->type));
				break;
		}
		
		if (!err) err = atomerr;
	}


	// Extra checks
	switch (tir->mediaType) {
		case 'vide':
			if (tir->trackVolume) {
				errprint("Video track has non-zero trackVolume\n");
				err = badAtomSize;
			}
			if ((tir->trackWidth==0) || (tir->trackHeight==0)) {
				errprint("Video track has zero trackWidth and/or trackHeight\n");
				err = badAtomSize;
			}
			if (vg.checklevel >= checklevel_samples) {
				UInt64 sampleOffset;
				UInt32 sampleSize;
				UInt32 sampleDescriptionIndex;
				Ptr dataP = nil;
				BitBuffer bb;
				
				sampleprint("<vide_SAMPLE_DATA>\n"); vg.tabcnt++;
					for (i = 1; i <= tir->sampleSizeEntryCnt; i++) {
						if ((vg.samplenumber==0) || (vg.samplenumber==i)) {
							err = GetSampleOffsetSize( tir, i, &sampleOffset, &sampleSize, &sampleDescriptionIndex );
							sampleprint("<sample num=\"%d\" offset=\"%s\" size=\"%d\" />\n",i,int64toxstr(sampleOffset),sampleSize); vg.tabcnt++;
							BAILIFNIL( dataP = malloc(sampleSize), allocFailedErr );
							err = GetFileData( vg.fileaoe, dataP, sampleOffset, sampleSize, nil );
							
							BitBuffer_Init(&bb, (void *)dataP, sampleSize);

							Validate_vide_sample_Bitstream( &bb, tir );
							free( dataP );
							--vg.tabcnt; sampleprint("</sample>\n");
						}
					}
				--vg.tabcnt; sampleprint("</vide_SAMPLE_DATA>\n");
			}
			break;

		case 'soun':
			if (tir->trackWidth || tir->trackHeight) {
				errprint("Sound track has non-zero trackWidth and/or trackHeight\n");
				err = badAtomSize;
			}
			if (vg.checklevel >= checklevel_samples) {
				UInt64 sampleOffset;
				UInt32 sampleSize;
				UInt32 sampleDescriptionIndex;
				Ptr dataP = nil;
				BitBuffer bb;
				
				sampleprint("<audi_SAMPLE_DATA>\n"); vg.tabcnt++;
					for (i = 1; i <= tir->sampleSizeEntryCnt; i++) {
						if ((vg.samplenumber==0) || (vg.samplenumber==i)) {
							err = GetSampleOffsetSize( tir, i, &sampleOffset, &sampleSize, &sampleDescriptionIndex );
							sampleprint("<sample num=\"%d\" offset=\"%s\" size=\"%d\" />\n",i,int64toxstr(sampleOffset),sampleSize); vg.tabcnt++;
							BAILIFNIL( dataP = malloc(sampleSize), allocFailedErr );
							err = GetFileData( vg.fileaoe, dataP, sampleOffset, sampleSize, nil );
							
							BitBuffer_Init(&bb, (void *)dataP, sampleSize);

							Validate_soun_sample_Bitstream( &bb, tir );
							free( dataP );
							--vg.tabcnt; sampleprint("</sample>\n");
						}
					}
				--vg.tabcnt; sampleprint("</audi_SAMPLE_DATA>\n");
			}
			break;
			
		case 'odsm':
			if (tir->trackVolume || tir->trackWidth || tir->trackHeight) {
				errprint("ObjectDescriptor track has non-zero trackVolume, trackWidth, or trackHeight\n");
				err = badAtomSize;
			}
			if (vg.checklevel >= checklevel_samples) {
				UInt64 sampleOffset;
				UInt32 sampleSize;
				UInt32 sampleDescriptionIndex;
				Ptr dataP = nil;
				BitBuffer bb;
				
				sampleprint("<odsm_SAMPLE_DATA>\n"); vg.tabcnt++;
				for (i = 1; i <= tir->sampleSizeEntryCnt; i++) {
					if ((vg.samplenumber==0) || (vg.samplenumber==i)) {
						err = GetSampleOffsetSize( tir, i, &sampleOffset, &sampleSize, &sampleDescriptionIndex );
						sampleprint("<sample num=\"%d\" offset=\"%s\" size=\"%d\" />\n",1,int64toxstr(sampleOffset),sampleSize); vg.tabcnt++;
							BAILIFNIL( dataP = malloc(sampleSize), allocFailedErr );
							err = GetFileData( vg.fileaoe, dataP, sampleOffset, sampleSize, nil );
							
							BitBuffer_Init(&bb, (void *)dataP, sampleSize);

							Validate_odsm_sample_Bitstream( &bb, tir );
							free( dataP );
						--vg.tabcnt; sampleprint("</sample>\n");
					}
				}
				--vg.tabcnt; sampleprint("</odsm_SAMPLE_DATA>\n");
			}
			break;

		case 'sdsm':
			if (tir->trackVolume || tir->trackWidth || tir->trackHeight) {
				errprint("SceneDescriptor track has non-zero trackVolume, trackWidth, or trackHeight\n");
				err = badAtomSize;
			}
			if (vg.checklevel >= checklevel_samples) {
				UInt64 sampleOffset;
				UInt32 sampleSize;
				UInt32 sampleDescriptionIndex;
				Ptr dataP = nil;
				BitBuffer bb;
				sampleprint("<sdsm_SAMPLE_DATA>\n"); vg.tabcnt++;
				for (i = 1; i <= tir->sampleSizeEntryCnt; i++) {
					if ((vg.samplenumber==0) || (vg.samplenumber==i)) {
						err = GetSampleOffsetSize( tir, i, &sampleOffset, &sampleSize, &sampleDescriptionIndex );
						sampleprint("<sample num=\"%d\" offset=\"%s\" size=\"%d\" />\n",1,int64toxstr(sampleOffset),sampleSize); vg.tabcnt++;
							BAILIFNIL( dataP = malloc(sampleSize), allocFailedErr );
							err = GetFileData( vg.fileaoe, dataP, sampleOffset, sampleSize, nil );
							
							BitBuffer_Init(&bb, (void *)dataP, sampleSize);

							Validate_sdsm_sample_Bitstream( &bb, tir);
							free( dataP );
						--vg.tabcnt; sampleprint("</sample>\n");
					}
				}
				--vg.tabcnt; sampleprint("</sdsm_SAMPLE_DATA>\n");
			}
			break;

		case 'hint':
			Validate_Hint_Track(aoe, tir);
			break;

		default:
			if (tir->trackVolume || tir->trackWidth || tir->trackHeight) {
				errprint("Non-visual/audio track has non-zero trackVolume, trackWidth, or trackHeight\n");
				err = badAtomSize;
			}
			break;
	}
	
	aoe->aoeflags |= kAtomValidated;
bail:
	return err;
}
//==========================================================================================

OSErr Validate_stbl_Atom( atomOffsetEntry *aoe, void *refcon )
{
	OSErr err = noErr;
	long cnt;
	atomOffsetEntry *list;
	long i;
	OSErr atomerr = noErr;
	long mvhdCnt = 0;
	long trakCnt = 0;
	long iodsCnt = 0;
	atomOffsetEntry *entry;
	UInt64 minOffset, maxOffset;
	TrackInfoRec *tir = (TrackInfoRec *)refcon;
	
	atomprintnotab(">\n"); 
	
	minOffset = aoe->offset + aoe->atomStartSize;
	maxOffset = aoe->offset + aoe->size - aoe->atomStartSize;
	
	BAILIFERR( FindAtomOffsets( aoe, minOffset, maxOffset, &cnt, &list ) );
	
	// Process 'stsd' atoms
	atomerr = ValidateAtomOfType( 'stsd', kTypeAtomFlagMustHaveOne | kTypeAtomFlagCanHaveAtMostOne, 
		Validate_stsd_Atom, cnt, list, tir );
	if (!err) err = atomerr;

	// Process 'stts' atoms
	atomerr = ValidateAtomOfType( 'stts', kTypeAtomFlagMustHaveOne | kTypeAtomFlagCanHaveAtMostOne, 
		Validate_stts_Atom, cnt, list, tir );
	if (!err) err = atomerr;

	// Process 'ctts' atoms
	atomerr = ValidateAtomOfType( 'ctts', kTypeAtomFlagCanHaveAtMostOne, 
		Validate_ctts_Atom, cnt, list, tir );
	if (!err) err = atomerr;

	// Process 'stss' atoms
	atomerr = ValidateAtomOfType( 'stss', kTypeAtomFlagCanHaveAtMostOne, 
		Validate_stss_Atom, cnt, list, tir );
	if (!err) err = atomerr;

	// Process 'stsc' atoms
	atomerr = ValidateAtomOfType( 'stsc', kTypeAtomFlagMustHaveOne | kTypeAtomFlagCanHaveAtMostOne, 
		Validate_stsc_Atom, cnt, list, tir );
	if (!err) err = atomerr;

	// Process 'stsz' atoms
	atomerr = ValidateAtomOfType( 'stsz', /* kTypeAtomFlagMustHaveOne | */  kTypeAtomFlagCanHaveAtMostOne, 
		Validate_stsz_Atom, cnt, list, tir );
	if (!err) err = atomerr;

	// Process 'stz2' atoms;  we need to check there is one stsz or one stz2 but not both...
	atomerr = ValidateAtomOfType( 'stz2', /* kTypeAtomFlagMustHaveOne | */  kTypeAtomFlagCanHaveAtMostOne, 
		Validate_stz2_Atom, cnt, list, tir );
	if (!err) err = atomerr;

	// Process 'stco' atoms
	atomerr = ValidateAtomOfType( 'stco', /* kTypeAtomFlagMustHaveOne | */ kTypeAtomFlagCanHaveAtMostOne, 
		Validate_stco_Atom, cnt, list, tir );
	if (!err) err = atomerr;

	// Process 'co64' atoms
	atomerr = ValidateAtomOfType( 'co64', /* kTypeAtomFlagMustHaveOne | */ kTypeAtomFlagCanHaveAtMostOne, 
		Validate_co64_Atom, cnt, list, tir );
	if (!err) err = atomerr;

	// Process 'stsh' atoms	- shadow sync
	atomerr = ValidateAtomOfType( 'stsh', kTypeAtomFlagCanHaveAtMostOne, 
		Validate_stsh_Atom, cnt, list, tir );
	if (!err) err = atomerr;

	// Process 'stdp' atoms	- degradation priority
	atomerr = ValidateAtomOfType( 'stdp', kTypeAtomFlagCanHaveAtMostOne, 
		Validate_stdp_Atom, cnt, list, tir );
	if (!err) err = atomerr;

	// Process 'sdtp' atoms	- sample dependency
	atomerr = ValidateAtomOfType( 'sdtp', kTypeAtomFlagCanHaveAtMostOne, 
		Validate_sdtp_Atom, cnt, list, tir );
	if (!err) err = atomerr;

	// Process 'padb' atoms
	atomerr = ValidateAtomOfType( 'padb', kTypeAtomFlagCanHaveAtMostOne, 
		Validate_padb_Atom, cnt, list, tir );
	if (!err) err = atomerr;

	//
	for (i = 0; i < cnt; i++) {
		entry = &list[i];

		if (entry->aoeflags & kAtomValidated) continue;

		switch (entry->type) {
			default:
				warnprint("WARNING: unknown sample table atom '%s'\n",ostypetostr(entry->type));
				break;
		}
		
		if (!err) err = atomerr;
	}
	
	if (tir->sampleSizeEntryCnt != tir->timeToSampleSampleCnt) {
		errprint("Number of samples described by SampleSize table ('stsz') does NOT match"
				 " number of samples described by TimeToSample table ('stts') \n");
		err = badAtomErr;
	}
	if (tir->mediaDuration != tir->timeToSampleDuration) {
	
		errprint("Media duration (%s) in MediaHeader does NOT match"
				 " sum of durations described by TimeToSample table (%s) \n", 
				  int64todstr_r( tir->mediaDuration, tempStr1 ),
				  int64todstr_r( tir->timeToSampleDuration, tempStr2 ));
		err = badAtomErr;
	}
	if (tir->sampleToChunk) {
		UInt32 s;		// number of samples
		UInt32 leftover;

		if (tir->sampleToChunk[tir->sampleToChunkEntryCnt].firstChunk 
			> tir->chunkOffsetEntryCnt) {
			errprint("SampleToChunk table describes more chunks than"
					 " the ChunkOffsetTable table\n");
			err = badAtomErr;
		} 
		
		s = tir->sampleSizeEntryCnt - tir->sampleToChunkSampleSubTotal;
		leftover = s % (tir->sampleToChunk[tir->sampleToChunkEntryCnt].samplesPerChunk);
		if (leftover) {
			errprint("SampleToChunk table does not evenly describe"
					 " the number of samples as defined by the SampleToSize table\n");
			err = badAtomErr;
		}
	}

	aoe->aoeflags |= kAtomValidated;
bail:
	return err;
}
//==========================================================================================


//==========================================================================================

OSErr ValidateAtomOfType( OSType theType, long flags, ValidateAtomTypeProcPtr validateProc, 
		long cnt, atomOffsetEntry *list, void *refcon )
{
	long i;
	OSErr err = noErr;
	char cstr[5] = {0};
	long typeCnt = 0;
	atomOffsetEntry *entry;
	OSErr atomerr;
	atompathType curatompath;
	Boolean curatomprint;
	Boolean cursampleprint;
	
	cstr[0] = (theType >> 24) & 0xff;
	cstr[1] = (theType >> 16) & 0xff;
	cstr[2] = (theType >>  8) & 0xff;
	cstr[3] = (theType >>  0) & 0xff;
	
	for (i = 0; i < cnt; i++) {
		entry = &list[i];
		
		if (entry->aoeflags & kAtomValidated) continue;
		
		if ((entry->type == theType) && ((entry->aoeflags & kAtomSkipThisAtom) == 0)) {
			if ((flags & kTypeAtomFlagCanHaveAtMostOne) && (typeCnt > 1)) {
				errprint("Multiple '%s' atoms not allowed\n", cstr);
			}
			if ((flags & kTypeAtomFlagMustBeFirst) && (i>0)) {
				if (i==1) warnprint("Warning: atom %s before ftyp atom MUST be a signature\n",ostypetostr((&list[0])->type));
				else errprint("Atom %s must be first and is actually at position %d\n",ostypetostr(theType),i+1);
			}			
			typeCnt++;
			addAtomToPath( vg.curatompath, theType, typeCnt, curatompath );
			if (vg.print_atompath) {
				fprintf(stdout,"%s\n", vg.curatompath);
			}
			curatomprint = vg.printatom;
			cursampleprint = vg.printsample;
			if ((vg.atompath[0] == 0) || (strcmp(vg.atompath,vg.curatompath) == 0)) {
				if (vg.print_atom)
					vg.printatom = true;
				if (vg.print_sample)
					vg.printsample = true;
			}
			atomprint("<%s",cstr); vg.tabcnt++;
				atomerr = CallValidateAtomTypeProc(validateProc, entry, 
											entry->refconOverride?((void*) (entry->refconOverride)):refcon);
			--vg.tabcnt; atomprint("</%s>\n",cstr); 
			vg.printatom = curatomprint;
			vg.printsample = cursampleprint;
			restoreAtomPath( vg.curatompath, curatompath );
			if (!err) err = atomerr;
		}
	}

	// 
	if ((flags & kTypeAtomFlagMustHaveOne)  && (typeCnt == 0)) {
		if( theType == IODSAID )
			warnprint( "\nWARNING: no 'iods' atom\n");
		else
			errprint("No '%s' atoms\n",cstr);
	} else if ((flags & kTypeAtomFlagCanHaveAtMostOne) && (typeCnt > 1)) {
		errprint("Multiple '%s' atoms not allowed\n",cstr);
	}

bail:
	return err;
}


//==========================================================================================



OSErr Validate_ftyp_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	UInt64 offset;
	OSType majorBrand;
	UInt32 version;
	UInt32 compatBrandListSize, numCompatibleBrands;
	char tempstr1[5], tempstr2[5];
	
	offset = aoe->offset + aoe->atomStartSize;
	
	BAILIFERR( GetFileDataN32( aoe, &majorBrand, offset, &offset ) );
	BAILIFERR( GetFileDataN32( aoe, &version, offset, &offset ) );

	atomprintnotab(" majorbrand=\"%.4s\" version=\"%s\", compatible_brands=[\n", ostypetostr_r(majorBrand, tempstr1), 
						int64toxstr((UInt64) version));

	vg.majorBrand = majorBrand;
	if( majorBrand == brandtype_isom ) {
		// the isom can only be a compatible brand
		errprint("The brand 'isom' can only be a compatible, not major, brand\n");
	}
	
	compatBrandListSize = (aoe->size - 8 - aoe->atomStartSize);
	numCompatibleBrands = compatBrandListSize / sizeof(OSType);
	
	if (0 != (compatBrandListSize % sizeof(OSType))) {
		errprint("FileType compatible brands array has leftover %d bytes\n", compatBrandListSize % sizeof(OSType));
	}
	if (numCompatibleBrands <= 0) {
		// must have at least one compatible brand, it must be the major brand
		errprint("There must be at least one compatible brand\n");
	}
	else {
		int ix;
		OSType currentBrand;
		Boolean majorBrandFoundAmongCompatibleBrands = false;
		
		for (ix=0; ix < numCompatibleBrands; ix++) {
			BAILIFERR( GetFileDataN32( aoe, &currentBrand, offset, &offset ) );
			if (ix<(numCompatibleBrands-1)) atomprint("\"%s\",\n", ostypetostr_r(currentBrand, tempstr1));
			      else atomprint("\"%s\"\n",  ostypetostr_r(currentBrand, tempstr1));
			
			if (majorBrand == currentBrand) {
				majorBrandFoundAmongCompatibleBrands = true;
			}
			
			
			
		}

		if (!majorBrandFoundAmongCompatibleBrands) {
				
				errprint("major brand ('%.4s') not also found in list of compatible brands\n", 
						     ostypetostr_r(majorBrand,tempstr2));
			}

			
 	}
 	
 	atomprint("]>\n"); 
	
	aoe->aoeflags |= kAtomValidated;

bail:
	return noErr;
}

typedef struct track_track {
	UInt32 chunk_num;
	UInt32 chunk_cnt;
} track_track;

OSErr Validate_moov_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	long cnt;
	atomOffsetEntry *list;
	long i;
	OSErr atomerr = noErr;
	long mvhdCnt = 0;
	long trakCnt = 0;
	long thisTrakIndex = 0;
	long iodsCnt = 0;
	atomOffsetEntry *entry;
	UInt64 minOffset, maxOffset;
	MovieInfoRec		*mir = NULL;
	
	atomprintnotab(">\n"); 
	
	minOffset = aoe->offset + aoe->atomStartSize;
	maxOffset = aoe->offset + aoe->size - aoe->atomStartSize;
	
	BAILIFERR( FindAtomOffsets( aoe, minOffset, maxOffset, &cnt, &list ) );
	

	// find out how many tracks we have so we can allocate our struct
	for (i = 0; i < cnt; i++) {
		entry = &list[i];
		if (entry->type == 'trak') {
			++trakCnt;
		}
	}
	
	if (trakCnt > 0) {
		i = (trakCnt-1) * sizeof(TrackInfoRec);
	} else {
		i = 0;
	}

	BAILIFNIL( vg.mir = calloc(1, sizeof(MovieInfoRec) + i), allocFailedErr );
	mir = vg.mir;
	mir->maxTIRs = trakCnt;


	atomerr = ValidateAtomOfType( 'mvhd', kTypeAtomFlagMustHaveOne | kTypeAtomFlagCanHaveAtMostOne, 
		Validate_mvhd_Atom, cnt, list, NULL);
	if (!err) err = atomerr;


	// pre-process 'trak' atoms - get the track types
	// set refconOverride so Validate_trak_Atom gets a tir
	thisTrakIndex = 0;
	for (i = 0; i < cnt; i++) {
		entry = &list[i];
		if (entry->type == 'trak') {
			++(mir->numTIRs);
			atomerr = Get_trak_Type(entry, &(mir->tirList[thisTrakIndex]));
			entry->refconOverride = (void*)&(mir->tirList[thisTrakIndex]);
			++thisTrakIndex;
		}
	}
		
	// disable processing hint 'trak' atoms
	//   adding ability to flag a text track to avoid reporting error when its matrix is non-identity
	thisTrakIndex = 0;
	for (i = 0; i < cnt; i++) {
		entry = &list[i];
		if (entry->type == 'trak') {
			if (mir->tirList[thisTrakIndex].mediaType == 'hint') {
				entry->aoeflags |= kAtomSkipThisAtom;
			}
			//    need to pass info that this is a text track to ValidateAtomOfType 'trak' below (refcon arg doesn't seem to work)
	
// ¥¥¥¥
			++thisTrakIndex;
		}
	}


	// Process non-hint 'trak' atoms
	atomerr = ValidateAtomOfType( 'trak', 0, Validate_trak_Atom, cnt, list, nil );
	if (!err) err = atomerr;


	// enable processing hint 'trak' atoms
	thisTrakIndex = 0;
	for (i = 0; i < cnt; i++) {
		entry = &list[i];
		if (entry->type == 'trak') {
			if (mir->tirList[thisTrakIndex].mediaType == 'hint') {
				entry->aoeflags &= ~kAtomSkipThisAtom;
			}

// ¥¥¥¥
			++thisTrakIndex;
		}
	}


	// Process hint 'trak' atoms
	atomerr = ValidateAtomOfType( 'trak', 0, Validate_trak_Atom, cnt, list, nil );
	if (!err) err = atomerr;
	
	// Process 'iods' atoms
	atomerr = ValidateAtomOfType( 'iods', kTypeAtomFlagMustHaveOne | kTypeAtomFlagCanHaveAtMostOne, 
		Validate_iods_Atom, cnt, list, nil );
	if (!err) err = atomerr;

	// Process 'udta' atoms
	atomerr = ValidateAtomOfType( 'udta', 0, 
		Validate_udta_Atom, cnt, list, nil );
	if (!err) err = atomerr;

	// Process 'uuid' atoms
	atomerr = ValidateAtomOfType( 'uuid', 0, 
		Validate_uuid_Atom, cnt, list, nil );
	if (!err) err = atomerr;

	// Process 'meta' atoms
	atomerr = ValidateAtomOfType( 'meta', 0, 
		Validate_meta_Atom, cnt, list, nil );
	if (!err) err = atomerr;

	//
	for (i = 0; i < cnt; i++){
		entry = &list[i];

		if (entry->aoeflags & kAtomValidated) continue;

		switch (entry->type) {
			case 'mdat':
			case 'skip':
			case 'free':
				break;
				
			case 'wide':	// this guy is QuickTime specific
			// ¥¥ if !qt, mpeg may be unfamiliar
				break;
				
			default:
				warnprint("WARNING: unknown movie atom '%s'\n",ostypetostr(entry->type));
				break;
		}
		
		if (!err) err = atomerr;
	}
	
	for (i=0; i<mir->numTIRs; ++i) {
			TrackInfoRec *tir;
			UInt8 all_single;\
			UInt32 j;
			
			tir = &(mir->tirList[i]);
			all_single = 1;
			
			if (tir->chunkOffsetEntryCnt > 1) {
				for (j=1; j<=tir->sampleToChunkEntryCnt; j++) {
					if (tir->sampleToChunk[j].samplesPerChunk > 1) 
						{ all_single = 0; break; }
				}
				if (all_single == 1) warnprint("Warning: track %d has %d chunks all containing 1 sample only\n",
												i,tir->chunkOffsetEntryCnt );
			}
		}
		
	// Check for overlapped sample chunks [dws]
	//  this re-write relies on the fact that most tracks are in offset order and most files behave;
	//  we pick the track with the lowest unprocessed chunk offset;
	//  if that is beyond the highest chunk end we have seen, we append it;  otherwise (the rare case)
	//   we insert it into the sorted list.  this gives us a rapid check and an output sorted list without
	//   an n-squared overlap check and without a post-sort
	//if (mir->numTIRs > 0)
	{
		UInt32 totalChunks = 0;
		TrackInfoRec *tir;
		UInt64 highwatermark;
		UInt32 trk_cnt, topslot = 0;
		track_track *trk;
		
		chunkOverlapRec *corp;
		
		UInt8 done = 0;
		
		trk_cnt = mir->numTIRs;
		
		BAILIFNULL( trk = calloc(trk_cnt,sizeof(track_track)), allocFailedErr );

		for (i=0; i<trk_cnt; ++i) {
			// find the chunk counts for each track and setup structures
			tir = &(mir->tirList[i]);
			totalChunks += tir->chunkOffsetEntryCnt;
			
			trk[i].chunk_cnt = tir->chunkOffsetEntryCnt;
			trk[i].chunk_num = 1;	// the next chunk to work on for each track
			
		}
		BAILIFNULL( corp = calloc(totalChunks,sizeof(chunkOverlapRec)), allocFailedErr );
				
		highwatermark = 0;		// the highest chunk end seen

		do { // until we have processed all chunks of all tracks
			UInt32 lowest;
			UInt64 low_offset;
			UInt64 chunkOffset, chunkStop;
			UInt32 chunkSize;
			UInt32 slot;
	
			// find the next lowest chunk start
			lowest = -1;		// next chunk not identified
			for (i=0; i<trk_cnt; i++) {
				UInt64 offset;
				tir = &(mir->tirList[i]);
				if (trk[i].chunk_num <= trk[i].chunk_cnt) {		// track has chunks to process
					offset = tir->chunkOffset[ trk[i].chunk_num ].chunkOffset;
					if ((lowest == -1)  || ((lowest != -1) && (offset<low_offset)))
					{
						low_offset = offset;
						lowest = i;
					}
				}
			}
			
			if (lowest == -1) {
				errprint("aargh: program error!!!\n");
				BAILIFERR( programErr );
			}
						
			tir = &(mir->tirList[lowest]);
			BAILIFERR( GetChunkOffsetSize(tir, trk[lowest].chunk_num, &chunkOffset, &chunkSize, nil) );
			if (chunkSize == 0) {
				errprint("Tracks with zero length chunks\n");
				err = badPublicMovieAtom;
				goto bail;
			}
			chunkStop = chunkOffset + chunkSize -1;
			
			if (chunkOffset != low_offset) {
				errprint("Aargh! program error\n");
				BAILIFERR( programErr );
			}
			
			if (chunkOffset >= vg.inMaxOffset) 
			{
				errprint("Chunk offset %s is at or beyond file size  0x%lx\n", int64toxstr(chunkOffset), vg.inMaxOffset);
			} else if (chunkStop > vg.inMaxOffset) 
			{
				errprint("Chunk end %s is beyond file size  0x%lx\n", int64toxstr(chunkStop), vg.inMaxOffset);
			}
			
			if (chunkOffset >= highwatermark)
			{	// easy, it starts after all other chunks end
				slot = topslot;
			} 
			else 
			{
				// have to insert the chunk into the list somewhere; it might overlap
				UInt32 k, priorslot, nextslot;
				
				// find the first chunk we already have that is starts after the candidate starts (if any)
				slot = topslot;
				for (k=0; k<topslot; k++) {
					// this could be done with binary chop, but it happens rarely
					if (corp[k].chunkStart > chunkOffset) { 
						slot = k; 
						break; 
					}
				}
				
				// Note we only warn if hint track chunks share data with other chunks, of if
				//  two tracks of the same type share data
				
				// do we overlap the prior slots (if any)?
				//   we might overlap slots before that, but if so, they must also overlap the slot
				//   prior to us, and we would have already reported that error
				if (slot > 0) {
					priorslot = slot-1;
					if ((chunkOffset >= corp[priorslot].chunkStart) && (chunkOffset <= corp[priorslot].chunkStop)) {
						if ((tir->mediaType == corp[priorslot].mediaType) || 
						    (tir->mediaType == 'hint') || 
							(corp[priorslot].mediaType == 'hint')) 
						warnprint("Warning: chunk %d of track ID %d at %s overlaps chunk from track ID %d at %s\n",
							trk[lowest].chunk_num, tir->trackID, int64todstr_r( chunkOffset, tempStr1 ), 
							corp[priorslot].trackID, int64todstr_r( corp[priorslot].chunkStart, tempStr2 ));
						else errprint("Error: chunk %d of track ID %d at %s overlaps chunk from track ID %d at %s\n",
							trk[lowest].chunk_num, tir->trackID, int64todstr_r( chunkOffset, tempStr1 ), 
							corp[priorslot].trackID, int64todstr_r( corp[priorslot].chunkStart, tempStr2 ));
					}
				}

				// do we overlap the next slots (if any)?
				//   again, we might overlap slots after that, but if so, we also overlap the next slot
				//   and one report is enough
				if (slot < topslot) {
					if ((chunkStop >= corp[slot].chunkStart) && (chunkStop <= corp[slot].chunkStop)) {
						if ((tir->mediaType == corp[slot].mediaType) || 
						    (tir->mediaType == 'hint') || 
							(corp[slot].mediaType == 'hint')) 
						warnprint("Warning: chunk %d of track ID %d at %s overlaps chunk from track ID %d at %s\n",
							trk[lowest].chunk_num, tir->trackID, int64todstr_r( chunkOffset, tempStr1 ), 
							corp[slot].trackID, int64todstr_r( corp[slot].chunkStart, tempStr2 ));
						else errprint("Error: chunk %d of track ID %d at %s overlaps chunk from track ID %d at %s\n",
							trk[lowest].chunk_num, tir->trackID, int64todstr_r( chunkOffset, tempStr1 ), 
							corp[slot].trackID, int64todstr_r( corp[slot].chunkStart, tempStr2 ));
					}
				}
					
				// now shuffle the array up prior to inserting this record into the sorted list
				for (nextslot = topslot; nextslot>slot; nextslot-- ) 
					corp[nextslot] = corp[ nextslot-1 ];
			}
			corp[ slot ].chunkStart = chunkOffset;
			corp[ slot ].chunkStop  = chunkStop;
			corp[ slot ].trackID 	= tir->trackID;
			corp[ slot ].mediaType  = tir->mediaType;  
			
				
			if (chunkStop > highwatermark) highwatermark = chunkStop;
			topslot++;

			trk[lowest].chunk_num += 1;		// done that chunk
			
			// see whether we have eaten all chunks for all tracks			
			done = 1;
			for (i=0; i<trk_cnt; i++) {
				if (trk[i].chunk_num <= trk[i].chunk_cnt) { done = 0; break; }
			}
		} while (done != 1);
		// until we have processed all chunks of all tracks
	}
			
	aoe->aoeflags |= kAtomValidated;
bail:
//	if (mir != NULL) {
//		dispose_mir(mir);
//	}
	return err;
}

void dispose_mir( MovieInfoRec *mir )
{
	// for each track, get rid of the stuff in it


	free( mir );
}

//==========================================================================================

//==========================================================================================

OSErr Validate_tref_Atom( atomOffsetEntry *aoe, void *refcon )
{
	OSErr err = noErr;
	long cnt;
	atomOffsetEntry *list;
	long i;
	OSErr atomerr = noErr;
	long mvhdCnt = 0;
	long trakCnt = 0;
	long iodsCnt = 0;
	atomOffsetEntry *entry;
	UInt64 minOffset, maxOffset;
	
	atomprintnotab(">\n"); 
	
	minOffset = aoe->offset + aoe->atomStartSize;
	maxOffset = aoe->offset + aoe->size - aoe->atomStartSize;
	
	BAILIFERR( FindAtomOffsets( aoe, minOffset, maxOffset, &cnt, &list ) );
	
	// Process 'tref_hint' atoms
	atomerr = ValidateAtomOfType( 'hint', kTypeAtomFlagCanHaveAtMostOne, 
		Validate_tref_hint_Atom, cnt, list, refcon );
	if (!err) err = atomerr;

	// Process 'tref_dpnd' atoms
	atomerr = ValidateAtomOfType( 'dpnd', kTypeAtomFlagCanHaveAtMostOne, 
		Validate_tref_dpnd_Atom, cnt, list, refcon );
	if (!err) err = atomerr;

	// Process 'tref_ipir' atoms
	atomerr = ValidateAtomOfType( 'ipir', kTypeAtomFlagCanHaveAtMostOne, 
		Validate_tref_ipir_Atom, cnt, list, refcon );
	if (!err) err = atomerr;

	// Process 'tref_mpod' atoms
	atomerr = ValidateAtomOfType( 'mpod', kTypeAtomFlagCanHaveAtMostOne, 
		Validate_tref_mpod_Atom, cnt, list, refcon );
	if (!err) err = atomerr;

	// Process 'tref_sync' atoms
	atomerr = ValidateAtomOfType( 'sync', kTypeAtomFlagCanHaveAtMostOne, 
		Validate_tref_sync_Atom, cnt, list, refcon );
	if (!err) err = atomerr;

	//
	for (i = 0; i < cnt; i++) {
		entry = &list[i];

		if (entry->aoeflags & kAtomValidated) continue;

		switch (entry->type) {
			default:
				warnprint("WARNING: unknown track reference atom '%s'\n",ostypetostr(entry->type));
				break;
		}
		
		if (!err) err = atomerr;
	}
	
	aoe->aoeflags |= kAtomValidated;
bail:
	return err;
}

//==========================================================================================

OSErr Validate_udta_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	long cnt;
	atomOffsetEntry *list;
	long i;
	OSErr atomerr = noErr;
	atomOffsetEntry *entry;
	UInt64 minOffset, maxOffset;
	
	atomprintnotab(">\n"); 
	
	minOffset = aoe->offset + aoe->atomStartSize;
	maxOffset = aoe->offset + aoe->size - aoe->atomStartSize;
	
	BAILIFERR( FindAtomOffsets( aoe, minOffset, maxOffset, &cnt, &list ) );
	
	// Process 'cprt' atoms
	atomerr = ValidateAtomOfType( 'cprt', 0,		// can have multiple copyright atoms 
		Validate_cprt_Atom, cnt, list, nil );
	if (!err) err = atomerr;

	// Process 'loci' atoms
	atomerr = ValidateAtomOfType( 'loci', 0,		// can have multiple copyright atoms 
								 Validate_loci_Atom, cnt, list, nil );
	if (!err) err = atomerr;


    // Process 'hnti' atoms
	atomerr = ValidateAtomOfType( 'hnti', kTypeAtomFlagCanHaveAtMostOne,
		Validate_moovhnti_Atom, cnt, list, nil );
	if (!err) err = atomerr;

	//
	for (i = 0; i < cnt; i++) {
		entry = &list[i];

		if (entry->aoeflags & kAtomValidated) continue;

		switch (entry->type) {
			default:
				warnprint("WARNING: unknown/unexpected atom '%s'\n",ostypetostr(entry->type));
				break;
		}
		
		if (!err) err = atomerr;
	}
	
	aoe->aoeflags |= kAtomValidated;
bail:
	return err;
}


//==========================================================================================

//==========================================================================================
//==========================================================================================

static OSErr Validate_rtp_Atom( atomOffsetEntry *aoe, void *refcon )
{
	OSErr 			err = noErr;
    UInt32			dataSize;
    char			*current;
	OSType			subType;
	Ptr				rtpDataP = NULL;
	Ptr				sdpDataP = NULL;
	UInt64			temp64;
    
    atomprintnotab(">\n"); 

	BAILIFNIL( rtpDataP = malloc((UInt32)aoe->size), allocFailedErr );

    dataSize = aoe->size - aoe->atomStartSize;
	BAILIFERR( GetFileData(aoe, rtpDataP, aoe->offset + aoe->atomStartSize, dataSize, &temp64) );
	
	current = rtpDataP;
	subType = EndianU32_BtoN(*((UInt32*)current));
	current += sizeof(UInt32);

    if (subType == 'sdp ') {
		// we found the sdp data
		// make a copy and null terminate it
		dataSize -= 4; // subtract the subtype field from the length 
		BAILIFNIL( sdpDataP = malloc(dataSize+1), allocFailedErr );
		memcpy(sdpDataP, current, dataSize);
		sdpDataP[dataSize] = '\0';
		
		BAILIFERR( Validate_Movie_SDP(sdpDataP) );
	} else {
		errprint("no sdp in movie user data\n");
		err = outOfDataErr;
		goto bail;
	}
    
bail:
	return err;
}

OSErr Validate_moovhnti_Atom( atomOffsetEntry *aoe, void *refcon )
{
	OSErr err = noErr;
	long cnt;
	atomOffsetEntry *list;
	long i;
	OSErr atomerr = noErr;
	atomOffsetEntry *entry;
	UInt64 minOffset, maxOffset;
	
	atomprintnotab(">\n"); 
			
	minOffset = aoe->offset + aoe->atomStartSize;
	maxOffset = aoe->offset + aoe->size - aoe->atomStartSize;
	
	BAILIFERR( FindAtomOffsets( aoe, minOffset, maxOffset, &cnt, &list ) );
	
	// Process 'rtp ' atoms
	atomerr = ValidateAtomOfType( 'rtp ', kTypeAtomFlagCanHaveAtMostOne, 
		Validate_rtp_Atom, cnt, list, NULL );
	if (!err) err = atomerr;
    
    for (i = 0; i < cnt; i++) {
		entry = &list[i];

		if (entry->aoeflags & kAtomValidated) continue;

		switch (entry->type) {
			default:
			// ¥¥ should warn
				break;
		}
		
		if (!err) err = atomerr;
	}
	
	aoe->aoeflags |= kAtomValidated;
bail:
	return err;
}

//==========================================================================================

OSErr Validate_sinf_Atom( atomOffsetEntry *aoe, void *refcon, UInt32 flags )
{
#pragma unused(refcon)
	OSErr err = noErr;
	long cnt;
	atomOffsetEntry *list;
	long i;
	OSErr atomerr = noErr;
	atomOffsetEntry *entry;
	UInt64 minOffset, maxOffset;
	
	atomprintnotab(">\n"); 
	
	minOffset = aoe->offset + aoe->atomStartSize;
	maxOffset = aoe->offset + aoe->size - aoe->atomStartSize;
	
	BAILIFERR( FindAtomOffsets( aoe, minOffset, maxOffset, &cnt, &list ) );
	
	// Process 'frma' atoms
	atomerr = ValidateAtomOfType( 'frma', flags | kTypeAtomFlagCanHaveAtMostOne, 
		Validate_frma_Atom, cnt, list, nil );
	if (!err) err = atomerr;

	// Process 'schm' atoms
	atomerr = ValidateAtomOfType( 'schm', kTypeAtomFlagCanHaveAtMostOne, 
		Validate_schm_Atom, cnt, list, nil );
	if (!err) err = atomerr;

	// Process 'schi' atoms
	atomerr = ValidateAtomOfType( 'schi', kTypeAtomFlagCanHaveAtMostOne, 
		Validate_schi_Atom, cnt, list, nil );
	if (!err) err = atomerr;

	for (i = 0; i < cnt; i++) {
		entry = &list[i];

		if (entry->aoeflags & kAtomValidated) continue;

		switch (entry->type) {				
			default:
				warnprint("WARNING: unknown security information atom '%s'\n",ostypetostr(entry->type));
				break;
		}
		
		if (!err) err = atomerr;
	}
	
	aoe->aoeflags |= kAtomValidated;
bail:
	return err;
}

//==========================================================================================

OSErr Validate_meta_Atom( atomOffsetEntry *aoe, void *refcon )
{
#pragma unused(refcon)
	OSErr err = noErr;
	long cnt;
	atomOffsetEntry *list;
	long i;
	OSErr atomerr = noErr;
	atomOffsetEntry *entry;
	UInt64 offset, minOffset, maxOffset;
	
	UInt32 version;
	UInt32 flags;

	atomprintnotab(">\n"); 
	
	// Get version/flags
	BAILIFERR( GetFullAtomVersionFlags( aoe, &version, &flags, &offset ) );
	atomprintnotab("\tversion=\"%d\" flags=\"%d\"\n", version, flags);
	FieldMustBe( version, 0, "version must be %d not %d" );
	FieldMustBe( flags, 0, "flags must be %d not %d" );

	minOffset = offset;
	maxOffset = aoe->offset + aoe->size;
	
	BAILIFERR( FindAtomOffsets( aoe, minOffset, maxOffset, &cnt, &list ) );
	
	// Process 'hdlr' atoms
	atomerr = ValidateAtomOfType( 'hdlr', kTypeAtomFlagMustHaveOne | kTypeAtomFlagCanHaveAtMostOne, 
		Validate_hdlr_Atom, cnt, list, nil );
	if (!err) err = atomerr;

	// Process 'pitm' atoms
	atomerr = ValidateAtomOfType( 'pitm', kTypeAtomFlagCanHaveAtMostOne, 
		Validate_pitm_Atom, cnt, list, nil );
	if (!err) err = atomerr;

	// Process 'dinf' atoms
	atomerr = ValidateAtomOfType( 'dinf', kTypeAtomFlagCanHaveAtMostOne, 
		Validate_dinf_Atom, cnt, list, nil );
	if (!err) err = atomerr;

	// Process 'iloc' atoms
	atomerr = ValidateAtomOfType( 'iloc', kTypeAtomFlagCanHaveAtMostOne, 
		Validate_iloc_Atom, cnt, list, nil );
	if (!err) err = atomerr;

	// Process 'ipro' atoms
	atomerr = ValidateAtomOfType( 'ipro', kTypeAtomFlagCanHaveAtMostOne, 
		Validate_ipro_Atom, cnt, list, nil );
	if (!err) err = atomerr;

	// Process 'iinf' atoms
	atomerr = ValidateAtomOfType( 'iinf', kTypeAtomFlagCanHaveAtMostOne, 
		Validate_iinf_Atom, cnt, list, nil );
	if (!err) err = atomerr;

	// Process 'xml ' atoms
	atomerr = ValidateAtomOfType( 'xml ', kTypeAtomFlagCanHaveAtMostOne, 
		Validate_xml_Atom, cnt, list, nil );
	if (!err) err = atomerr;

	// Process 'bxml' atoms
	atomerr = ValidateAtomOfType( 'bxml', kTypeAtomFlagCanHaveAtMostOne, 
		Validate_xml_Atom, cnt, list, nil );
	if (!err) err = atomerr;

	for (i = 0; i < cnt; i++) {
		entry = &list[i];

		if (entry->aoeflags & kAtomValidated) continue;

		switch (entry->type) {				
			default:
				warnprint("WARNING: unknown meta atom '%s'\n",ostypetostr(entry->type));
				break;
		}
		
		if (!err) err = atomerr;
	}
	
	aoe->aoeflags |= kAtomValidated;
bail:
	return err;
}


