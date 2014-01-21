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

	// JRM
extern ValidateGlobals vg;

OSErr Validate_ES_INC_Descriptor(BitBuffer *bb);
OSErr Validate_ES_REF_Descriptor(BitBuffer *bb);
OSErr Validate_ES_Descriptor(BitBuffer *bb, UInt8 Expect_ObjectType, UInt8 Expect_StreamType, Boolean fileForm);
OSErr Validate_Object_Descriptor(BitBuffer *inbb);
OSErr Validate_OCI_Descriptor(BitBuffer *bb);
OSErr Validate_IPMP_DescriptorPointer(BitBuffer *bb);
OSErr Validate_Extension_DescriptorPointer(BitBuffer *bb);
OSErr Validate_Dec_conf_Descriptor(BitBuffer *bb, UInt8 Expect_ObjectType, UInt8 Expect_StreamType);
OSErr Validate_SLconf_Descriptor( BitBuffer *bb, Boolean fileForm);
OSErr Validate_DecSpecific_Descriptor( BitBuffer *bb, UInt8 ObjectType, UInt8 StreamType, void *p_sc );
OSErr Validate_SoundSpecificInfo(  BitBuffer *bb );
OSErr Validate_BIFSSpecificInfo( BitBuffer *bb, UInt8 ObjectType );
OSErr Validate_VideoSpecificInfo(  BitBuffer *bb, UInt32 expect_startcode, UInt8 default_voVerID, void *p_sc );
UInt8 bitsizeof( UInt16 x );
OSErr ValidateQuantMat(BitBuffer *bb, char* matname);
OSErr CheckValuesInContext( UInt32 bufferSize, UInt32 maxBitrate, UInt32 avgBitrate, void *p_sc );


#define VALIDATE_FIELD(fmt, var, bits) do { \
	var = GetBits(bb, bits, &err); if (err) goto bail; \
	atomprint("%s=\"" fmt "\"\n", #var, var); \
	} while (false)
	
#define VALIDATE_INDEX_FIELD(fmt, var, bits, nam, idx) do { \
	var = GetBits(bb, bits, &err); if (err) goto bail; \
	atomprint(nam "_%s[%d]=\"" fmt "\"\n", #var, idx, var); \
	} while (false)

#define VALIDATE_INDEX_FIELD1(fmt, var, bits, idx) do { \
	var = GetBits(bb, bits, &err); if (err) goto bail; \
	atomprint("_%s[%d]=\"" fmt "\"\n", #var, idx, var); \
	} while (false)

#define VALIDATE_FIELD_V(fmt, var, bits, val, aname) do { \
	var = GetBits(bb, bits, &err); if (err) goto bail; \
	atomprint("%s=\"" fmt "\"\n", #var, var); \
	if ((var)!=(val)) { err = badAtomErr; errprint("Validate %s: %s != %d\n",aname,#var,(val)); } \
	} while (false)

#define VALIDATE_UEV(fmt, var) do { \
	var = read_golomb_uev(bb, &err); if (err) goto bail; \
	atomprint("%s=\"" fmt "\"\n", #var, var); \
	} while (false)
	
#define VALIDATE_SEV(fmt, var) do { \
	var = read_golomb_sev(bb, &err); if (err) goto bail; \
	atomprint("%s=\"" fmt "\"\n", #var, var); \
	} while (false)
	

//==========================================================================================

OSErr Validate_iods_OD_Bits( Ptr odDataP, unsigned long odSize, Boolean fileForm )
{
	OSErr err;
	BitBuffer thebb;
	BitBuffer* bb;

	UInt32 iodTag;
	UInt32 iodSize;
	UInt32 objectDescriptorID;
	UInt32 urlFlag;
	UInt32 reserved;
	Boolean includeInlineProfileLevelFlag;
	UInt32 ODProfileLevelIndication;
	UInt32 sceneProfileLevelIndication;
	UInt32 audioProfileLevelIndication;
	UInt32 visualProfileLevelIndication;
	UInt32 graphicsProfileLevelIndication;
	UInt32 tag;

	atomprint("<iods_OD"); vg.tabcnt++;
	
	bb = &thebb;
	
	BitBuffer_Init(bb, (void *)odDataP, odSize);

	BAILIFERR( GetDescriptorTagAndSize(bb, &iodTag, &iodSize) );
	atomprintnotab("\ttag=\"0x%2.2x\" size=\"%d\"\n", iodTag, iodSize);
    if (fileForm) {
        FieldMustBe(iodTag, Class_MP4_IOD_Tag, "ValidateIODSAtom: objectDescriptorTag != Class_MP4_IOD_Tag" );
    } else {
        FieldMustBe(iodTag, Class_InitialObjectDescTag, "ValidateIODSAtom: objectDescriptorTag != Class_InitialObjectDescTag" );

    }
	
	if (bb->bits_left != (iodSize*8)) errprint("ValidateIODSAtom: Descriptor doesn't fit exactly into atom!\n");
	
	VALIDATE_FIELD  ("%d",  objectDescriptorID, 10 );
	VALIDATE_FIELD  ("%d",  urlFlag, 1 );
	VALIDATE_FIELD  ("%d",  includeInlineProfileLevelFlag, 1 );
	VALIDATE_FIELD_V("%d",  reserved, 4, 0xf, "ValidateIODS" );

	if (urlFlag) {
		UInt32 urlLength;
		char urlString[257] = {0};
		
		BAILIFERRSET( urlLength = GetBits(bb, 8, &err) );
		BAILIFERR( GetBytes(bb, urlLength, (unsigned char*)urlString) );
		urlString[urlLength] = 0;
		atomprint("urlString =\"%s\"\n", urlString);
		
		atomprint(">\n");
	
	} else {
		/*
		VALIDATE_FIELD  ("%2.2x",  ODProfileLevelIndication, 8 );
		if ((ODProfileLevelIndication!=0xFF) && (ODProfileLevelIndication!=0xFE))
			errprint("Validate_IODS: ISMA expects no-capability(0xFF) or maybe unspecified (0xFE) for ODProfileLevelIndication\n");
		VALIDATE_FIELD  ("%2.2x",  sceneProfileLevelIndication, 8 );
		if ((sceneProfileLevelIndication!=0xFF) && (sceneProfileLevelIndication!=0xFE))
			errprint("Validate_IODS: ISMA expects no-capability(0xFF) or maybe unspecified (0xFE) for sceneProfileLevelIndication\n");
		VALIDATE_FIELD  ("%2.2x",  audioProfileLevelIndication, 8 );
		if ((audioProfileLevelIndication!=0xFF) && (audioProfileLevelIndication!=0x0F) )
			errprint("Validate_IODS: ISMA expects no-capability(0xFF) or Hi-Quality@L2 (0x0F) for audioProfileLevelIndication\n");
		VALIDATE_FIELD  ("%2.2x",  visualProfileLevelIndication, 8 );
		if ((visualProfileLevelIndication!=0xFF) && (visualProfileLevelIndication!=0x03) && (visualProfileLevelIndication!=0xF3))
			errprint("Validate_IODS: ISMA expects no-capability(0xFF) or Simple@L1 (0x03) or AdvSimple@L3 (0xF3) for visualProfileLevelIndication\n");
		VALIDATE_FIELD  ("%2.2x",  graphicsProfileLevelIndication, 8 );
		if ((graphicsProfileLevelIndication!=0xFF) && (graphicsProfileLevelIndication!=0xFE))
			errprint("Validate_IODS: ISMA expects no-capability(0xFF) or maybe unspecified (0xFE) for graphicsProfileLevelIndication\n");
		*/
		static char* audio_profiles[] = {
						"Reserved for ISO use",
						"Main Audio Profile @L1",
						"Main Audio@L2",
						"Main Audio@L3",
						"Main Audio@L4",
						"Scalable Audio@L1",
						"Scalable Audio@L2",
						"Scalable Audio@L3",
						"Scalable Audio@L4",
						"Speech Audio@L1",
						"Speech Audio@L2",
						"Synthetic Audio@L1",
						"Synthetic Audio@L2",
						"Synthetic Audio@L3",
						"High Quality Audio@L1",
						"High Quality Audio@L2",
						"High Quality Audio@L3",
						"High Quality Audio@L4",
						"High Quality Audio@L5",
						"High Quality Audio@L6",
						"High Quality Audio@L7",
						"High Quality Audio@L8",
						"Low Delay Audio@L1",
						"Low Delay Audio@L2",
						"Low Delay Audio@L3",
						"Low Delay Audio@L4",
						"Low Delay Audio@L5",
						"Low Delay Audio@L6",
						"Low Delay Audio@L7",
						"Low Delay Audio@L8",
						"Natural Audio@L1",
						"Natural Audio@L2",
						"Natural Audio@L3",
						"Natural Audio@L4",
						"Mobile Audio Internetworking@L1",
						"Mobile Audio Internetworking@L2",
						"Mobile Audio Internetworking@L3",
						"Mobile Audio Internetworking@L4",
						"Mobile Audio Internetworking@L5",
						"Mobile Audio Internetworking@L6",
						"AAC@L1",
						"AAC@L2",
						"AAC@L4",
						"AAC@L5",
						"High Efficiency AAC@L2",
						"High Efficiency AAC@L3",
						"High Efficiency AAC@L4",
						"High Efficiency AAC@L5" };
		VALIDATE_FIELD  ("%2.2x",  ODProfileLevelIndication, 8 );
		if (ODProfileLevelIndication!=0xFF)
			errprint("Validate_IODS: ISMA expects no-capability(0xFF) for ODProfileLevelIndication\n");

		VALIDATE_FIELD  ("%2.2x",  sceneProfileLevelIndication, 8 );
		if (sceneProfileLevelIndication!=0xFF)
			errprint("Validate_IODS: ISMA expects no-capability(0xFF) for sceneProfileLevelIndication\n");

		VALIDATE_FIELD  ("%2.2x",  audioProfileLevelIndication, 8 );
		if ((audioProfileLevelIndication!=0xFF) && 
			(audioProfileLevelIndication!=0x0F) && 
			(audioProfileLevelIndication!=0x0E) &&
			(audioProfileLevelIndication!=0x2a) && (audioProfileLevelIndication!=0x2c))
			errprint("Validate_IODS: ISMA expects no-capability(0xFF) or Hi-Quality@L1/L2 (0x0E, 0x0F) or AAC@L4 (0x2a) or HE-AAC@L2 (0x2c) for audioProfileLevelIndication\n");
		if (audioProfileLevelIndication<(sizeof(audio_profiles)/sizeof(char*))) 
			atomprint("  Comment=\"audio profile/level is %s\"\n",audio_profiles[audioProfileLevelIndication]);
	
		VALIDATE_FIELD  ("%2.2x",  visualProfileLevelIndication, 8 );
		if ((visualProfileLevelIndication!=0xFF) && 
			(visualProfileLevelIndication!=0x08) &&
			(!( (visualProfileLevelIndication>=0x01) && (visualProfileLevelIndication<=0x03) )) &&
			(!( (visualProfileLevelIndication>=0xf0) && (visualProfileLevelIndication<=0xf3) )) &&
			(visualProfileLevelIndication!=0xF7) &&
			(visualProfileLevelIndication!=0x7F))
			errprint("Validate_IODS: ISMA expects no-capability(0xFF) or Simple@L0-3 (0x08,01-03) or AdvSimple@L0-3b (0xF0-3,0xF7), or AVC (0x7f) for visualProfileLevelIndication\n");
		if( visualProfileLevelIndication != vg.visualProfileLevelIndication) {
		  if( vg.visualProfileLevelIndication == 0xFF )
			errprint("Validate_IODS: visualProfileLevelIndication ( IOD: %lu (0x%2.2x) ) signalled .. but there seems to be no video track\n",visualProfileLevelIndication, visualProfileLevelIndication);
		  else
			errprint("Validate_IODS: visualProfileLevelIndication ( IOD: %lu (0x%2.2x )) does not correspond to indication in sample description: %lu (0x%2.2x)\n",visualProfileLevelIndication, visualProfileLevelIndication, vg.visualProfileLevelIndication, vg.visualProfileLevelIndication);
		}

		VALIDATE_FIELD  ("%2.2x",  graphicsProfileLevelIndication, 8 );
		if (graphicsProfileLevelIndication!=0xFF)
			errprint("Validate_IODS: ISMA expects no-capability(0xFF) for graphicsProfileLevelIndication\n");

		atomprint(">\n");

		tag = PeekBits(bb, 8, nil);
		
        if (fileForm) {
            if (tag != Class_ES_ID_IncTag) {
                errprint("ValidateIODSAtom: must have at least one Class_ES_ID_IncTag\n");
            }
            while ( tag == Class_ES_ID_IncTag ) {
                Validate_ES_INC_Descriptor( bb);
                tag = PeekBits(bb, 8, nil);
            }
        } else { 
            if (tag != Class_ES_DescrTag) {
                errprint("ValidateIODSAtom: must have at least one Class_ES_ID_IncTag\n");
            }
            while ( tag == Class_ES_DescrTag ) {
                Validate_ES_Descriptor( bb, 0, 0, false);
                tag = PeekBits(bb, 8, nil);
            }
        }
		
		while ( (tag >= ExtDescrTagStartRange) && (tag <= ExtDescrTagEndRange) ) {
			Validate_OCI_Descriptor( bb);
			tag = PeekBits(bb, 8, nil);
		}
		while ( tag == Class_IPMP_DescrPointerTag ) {
			Validate_IPMP_DescriptorPointer( bb);
			tag = PeekBits(bb, 8, nil);
		}
	}
	tag = PeekBits(bb, 8, nil);
	while ( (tag >= OCIDescrTagStartRange) && (tag <= OCIDescrTagEndRange) ) {
		Validate_Extension_DescriptorPointer( bb);
		tag = PeekBits(bb, 8, nil);
	}

	--vg.tabcnt; atomprint("</iods_OD>\n"); 

	if (NumBytesLeft(bb) > 1) {
		err = tooMuchDataErr;
	}
	
bail:
	if (err) {
		errprint("ValidateIODSAtom: %d\n",err);
	}
	return err;
}

//==========================================================================================

OSErr Validate_ES_INC_Descriptor(BitBuffer *bb)
{
	OSErr err = noErr;
	UInt32 tag;
	UInt32 size;

	UInt32 ES_ID;

	atomprint("<ES_INC_descriptor"); vg.tabcnt++;

	if (bb->bits_left % 8) {
		errprint("Validate_ES_INC_Descriptor did not start byte aligned\n");
	}

	BAILIFERR( GetDescriptorTagAndSize(bb, &tag, &size) );
	atomprintnotab("\ttag=\"0x%2.2x\" size=\"%d\"\n", tag, size);
	FieldMustBe( size, 4, "Validate_ES_INC_Descriptor: size should be %d bytes not %d bytes\n" );
	
	VALIDATE_FIELD( "%ld", ES_ID, 32);
	/* !dws ES_ID should be the trackID of an existing OD or BIFS track */
	
	--vg.tabcnt; atomprint("/>\n");

bail:
	if (err) {
		errprint("Validate_ES_INC_Descriptor: %d\n",err);
	}
	return err;
}

//==========================================================================================

OSErr Validate_ES_REF_Descriptor(BitBuffer *bb)
{
	OSErr err = noErr;
	UInt32 tag;
	UInt32 size;

	UInt16 Trk_ref;

	atomprint("<ES_REF_descriptor"); vg.tabcnt++;

	if (bb->bits_left % 8) {
		errprint("Validate_ES_REF_Descriptor did not start byte aligned\n");
	}

	BAILIFERR( GetDescriptorTagAndSize(bb, &tag, &size) );
	atomprintnotab("\ttag=\"0x%2.2x\" size=\"%d\"\n", tag, size);
	FieldMustBe( size, 2, "Validate_ES_REF_Descriptor: size should be %d bytes not %d bytes\n" );
	
	VALIDATE_FIELD( "%ld", Trk_ref, 16);
	/* !dws trk_ref index ought to be in range */
	
	--vg.tabcnt; atomprint("/>\n");

bail:
	if (err) {
		errprint("Validate_ES_REF_Descriptor: %d\n",err);
	}
	return err;
}

//==========================================================================================

OSErr Validate_Dec_conf_Descriptor(BitBuffer *inbb, UInt8 Expect_ObjectType, UInt8 Expect_StreamType)
{
	OSErr err = noErr;
	BitBuffer newbb;
	BitBuffer *bb;
	UInt32 tag;
	UInt32 size;
	UInt8 ObjectType;
	UInt8 StreamType;
	UInt8 UpStream;
	UInt8 reserved;
	UInt32 buffersize;
	UInt32 maxbitrate;
	UInt32 avgbitrate;
		
	PartialVideoSC vsc, *p_vsc;
	p_vsc = &vsc;
	memset(p_vsc,0,sizeof(PartialVideoSC));

	
	atomprint("<DecConfig_descriptor"); vg.tabcnt++;
	
	bb = inbb;

	if (bb->bits_left % 8) {
		errprint("Validate_DecConf_Descriptor did not start byte aligned\n");
	}

	BAILIFERR( GetDescriptorTagAndSize(bb, &tag, &size) );
	atomprintnotab("\ttag=\"0x%2.2x\" size=\"%d\"\n", tag, size);
	FieldMustBe( tag, Class_DecoderConfigDescTag, "Validate_Dec_conf_Descriptor: Tag != Class_DecoderConfigDescTag\n" );

	newbb= *inbb;
	bb = &newbb;
	
	bb->bits_left = size*8;
	
	VALIDATE_FIELD("0x%02x",  ObjectType, 8 );
	if (Expect_ObjectType!=0) {
		FieldMustBe( ObjectType, Expect_ObjectType, "Validate_Dec_conf_Descriptor: expected ObjectType = 0x%lx, not 0x%lx\n" );
	}
	
	VALIDATE_FIELD("0x%02x",  StreamType, 6 );	
	if (Expect_StreamType != 0) {
		FieldMustBe( StreamType, Expect_StreamType, "Validate_Dec_conf_Descriptor: expected StreamType = 0x%lx, not 0x%lx\n" );
	}
	
	VALIDATE_FIELD_V("%d",  UpStream, 1, 0, "Dec_conf_Descriptor" );	
	VALIDATE_FIELD_V("%d",  reserved, 1, 1, "Dec_conf_Descriptor" );	
	VALIDATE_FIELD("%d",  buffersize, 24 );	
	VALIDATE_FIELD("%d",  maxbitrate, 32 );	
	VALIDATE_FIELD("%d",  avgbitrate, 32 );	

	if (bb->bits_left==0) {
		--vg.tabcnt;
		atomprint("/>\n");
	}
	else {
		atomprint(">\n");
		tag = PeekBits(bb, 8, nil);
	
		while ((bb->bits_left > 0) && ( tag == Class_DecSpecificInfoTag )) {
			Validate_DecSpecific_Descriptor( bb, ObjectType, StreamType, (void*)p_vsc );
			if( (ObjectType == 32) && (StreamType == 4) ){/* is video */
				 CheckValuesInContext(buffersize, maxbitrate, avgbitrate, (void*)p_vsc);
			}
			tag = PeekBits(bb, 8, nil);
		}	
		while (bb->bits_left > 0) {
			Validate_Random_Descriptor( bb, "Descriptor");
			tag = PeekBits(bb, 8, nil);
		}
	
		--vg.tabcnt; atomprint("</DecConfig_descriptor>\n");
	}
	err = SkipBytes(inbb, size); if (err) goto bail;

bail:
	if (err) {
		errprint("Validate_DecConf_Descriptor: %d\n",err);
	}
	return err;
}

//==========================================================================================

OSErr Validate_ES_Descriptor(BitBuffer *inbb, UInt8 Expect_ObjectType, UInt8 Expect_StreamType, Boolean fileForm)
{
	OSErr err = noErr;
	BitBuffer newbb;
	BitBuffer *bb;
	
	UInt32 tag;
	UInt32 size;
	UInt32 ES_ID;
	UInt8 streamDependenceFlag;
	UInt8 urlflag;
	UInt8 ocrstreamflag;
	UInt8 streampriority;

	atomprint("<ES_descriptor"); vg.tabcnt++;
	bb = inbb;
	
	if (bb->bits_left % 8) {
		errprint("Validate_ES_Descriptor did not start byte aligned\n");
	}

	BAILIFERR( GetDescriptorTagAndSize(bb, &tag, &size) );
	atomprintnotab("\ttag=\"0x%2.2x\" size=\"%d\"\n", tag, size);

		FieldMustBe( tag, Class_ES_DescrTag, "Validate_ES_Descriptor: Tag != Class_ES_DescrTag\n" );
	
	newbb= *inbb;
	bb = &newbb;
	
	bb->bits_left = size*8;
	
	VALIDATE_FIELD  ("%d",  ES_ID, 16 );
    if (fileForm) {
        FieldMustBe( ES_ID, 0, "Validate_ES_Descriptor: ES_ID should be %d not %d in media tracks\n" );
    }
	VALIDATE_FIELD_V("%d",  streamDependenceFlag, 1, 0, "ESDescriptor" );
	VALIDATE_FIELD  ("%d",  urlflag, 1 );
    if (fileForm) {
        VALIDATE_FIELD_V("%d",  ocrstreamflag, 1, 0, "ESDescriptor");
    } else {
        VALIDATE_FIELD  ("%d",  ocrstreamflag, 1);
    }
	VALIDATE_FIELD  ("%d",  streampriority, 5 );
	
	if (streamDependenceFlag==1) {
		UInt16 dependsOnESID;
		VALIDATE_FIELD("%d",  dependsOnESID, 16 );
	}
	if (urlflag) {
		UInt32 urlLength;
		char urlString[257] = {0};
        static const char* urlStartODAU = "data:application/mpeg4-od-au;base64,";
        static const char* urlStartBIFSAU = "data:application/mpeg4-bifs-au;base64,";
        char* base64P = NULL;
		
		urlLength = GetBits(bb, 8, &err); if (err) goto bail;
		GetBytes(bb, urlLength, (unsigned char*)urlString);
		urlString[urlLength] = 0;
		atomprint("URLString=\"%s\"\n", urlString);
        
        if (strncmp(urlString, urlStartODAU, strlen(urlStartODAU)) == 0) {
            base64P = urlString + strlen(urlStartODAU);
        } else if (strncmp(urlString, urlStartBIFSAU, strlen(urlStartBIFSAU)) == 0) {
            base64P = urlString + strlen(urlStartBIFSAU);
        }
        if (base64P) {
            UInt32 base64Size = strlen(base64P);
            UInt32 auSize = base64Size;	// more than enough, will be adjusted down
            Ptr auDataP = NULL;
            
            BAILIFNIL( auDataP = malloc(auSize), allocFailedErr );
            
            err = Base64DecodeToBuffer(base64P, &base64Size, auDataP, &auSize);
            if (err) {
                errprint("Validate_ES_Descriptor: URL bad base64 encoding");
            } else {
                BitBuffer aubb;
                
                BitBuffer_Init(&aubb, (void *)auDataP, auSize);
                
                if (strncmp(urlString, urlStartODAU, strlen(urlStartODAU)) == 0) {
                    Validate_odsm_sample_Bitstream( &aubb, NULL );
                } else { // if (strncmp(urlString, urlStartBIFSAU, strlen(urlStartBIFSAU)) == 0)
                    Validate_sdsm_sample_Bitstream( &aubb, NULL );
                } 
                
                free(auDataP);
            }
        }
    }
        
	if (ocrstreamflag==1) {
		UInt16 ocrESID;
		VALIDATE_FIELD("%d",  ocrESID, 16 );
	}
	
	atomprint(">\n");
		
	tag = PeekBits(bb, 8, nil);
	
	while ((bb->bits_left > 0) && ( tag == Class_DecoderConfigDescTag )) {
		Validate_Dec_conf_Descriptor( bb, Expect_ObjectType, Expect_StreamType );
		tag = PeekBits(bb, 8, nil);
	}	
	while ((bb->bits_left > 0) && ( tag == Class_SLConfigDescrTag )) {
		Validate_SLconf_Descriptor( bb, fileForm);
		tag = PeekBits(bb, 8, nil);
	}
	while (bb->bits_left > 0) 
		Validate_Random_Descriptor( bb, "Descriptor");

	err = SkipBytes(inbb, size); if (err) goto bail;
		
	--vg.tabcnt; atomprint("</ES_descriptor>\n");

bail:
	if (err) {
		errprint("Validate_ES_Descriptor: %d\n",err);
	}
	return err;
}

//==========================================================================================

OSErr Validate_Object_Descriptor(BitBuffer *inbb)
{
	OSErr err = noErr;
	BitBuffer newbb;
	BitBuffer *bb;
	UInt32 OD_ID;
	UInt32 resvd;
	UInt32 tag;
	UInt32 size;
	UInt8 urlflag;
	char* name;

	bb = inbb;
	
	if (bb->bits_left % 8) {
		errprint("Validate_Object_Descriptor did not start byte aligned\n");
	}

	BAILIFERR( GetDescriptorTagAndSize(bb, &tag, &size) );
	if (tag==Class_MP4_OD_Tag) name = "MP4_Object_descriptor"; else name = "Object_descriptor";
	atomprint("<%s", name); 
	vg.tabcnt++;

	atomprintnotab("\ttag=\"0x%2.2x\" size=\"%d\"\n", tag, size);

	FieldMustBeOneOf2( tag, UInt32, "Validate_Object_Descriptor: Tag != Class_ObjectDescrTag or MP4_OD_Tag\n",
						(Class_ObjectDescrTag, Class_MP4_OD_Tag) );
	newbb= *inbb;
	bb = &newbb;
	
	bb->bits_left = size*8;
	
	VALIDATE_FIELD  ("%d",  OD_ID, 10 );
	VALIDATE_FIELD  ("%d",  urlflag, 1 );
	VALIDATE_FIELD_V("%d",  resvd, 5, 31, "Validate_Object_Descriptor");

	if (urlflag) {
		UInt32 urlLength;
		char urlString[257] = {0};
		
		urlLength = GetBits(bb, 8, &err); if (err) goto bail;
		GetBytes(bb, urlLength, (unsigned char*)urlString);
		urlString[urlLength] = 0;
		atomprint("URLString=\"%s\"\n", urlString);
		while (bb->bits_left > 0) Validate_Random_Descriptor( bb, "Descriptor");
	}
	else {
		while (bb->bits_left > 0) {	
			tag = PeekBits(bb, 8, nil);
			switch (tag) {
				case Class_ES_DescrTag: 
					Validate_ES_Descriptor( bb, 0, 0, false );
					break;
					
				case Class_ES_ID_IncTag:
					Validate_ES_INC_Descriptor( bb );
					break;
					
				case Class_ES_ID_RefTag:
					Validate_ES_REF_Descriptor( bb );
					break;
					
				default: 
					Validate_Random_Descriptor( bb, "Descriptor");
					break;
				}
			}
		
	}

	err = SkipBytes(inbb, size); if (err) goto bail;
		
	--vg.tabcnt; atomprint("</%s>\n",name);

bail:
	if (err) {
		errprint("Validate_Object_Descriptor: %d\n",err);
	}
	return err;
}

//==========================================================================================

OSErr Validate_OCI_Descriptor(BitBuffer *bb)
{
	return Validate_Random_Descriptor( bb, "OCIDescriptor" );
}

//==========================================================================================

OSErr Validate_SLconf_Descriptor( BitBuffer *inbb, Boolean fileForm)
{
	OSErr err = noErr;
	BitBuffer newbb;
	BitBuffer *bb;
	
	UInt32 tag;
	UInt32 size;
	UInt8 predefined;
	
	UInt8 durationFlag, useTimeStampsFlag;
	UInt8 timeStampLength;	// must be <= 64

	atomprint("<SLConfigDescriptor"); vg.tabcnt++;
	
	bb = inbb;

	if (bb->bits_left % 8) {
		errprint("Validate SLConfigDescriptor did not start byte aligned\n");
	}

	BAILIFERR( GetDescriptorTagAndSize(bb, &tag, &size) );
	atomprintnotab("\ttag=\"0x%2.2x\" size=\"%d\"\n", tag, size);
	
	if (fileForm && size!=1) errprint("SLConfigDescriptor size %d is not required size 1\n",size);
	
	newbb=*inbb;
	bb=&newbb;
	bb->bits_left = size*8;
	
    if (fileForm) {
        VALIDATE_FIELD_V("%d",  predefined, 8, 2, "SLConfigDescriptor");
    } else {
        VALIDATE_FIELD("%d",  predefined, 8);
        if (predefined == 2) {
            errprint("SLConfigDescriptor predefined %d not allowed outside of mp4 file context\n", predefined);
        }
    }
	
	if (predefined==0) {
		UInt8 useAccessUnitStartFlag, useAccessUnitEndFlag, useRandomAccessPointFlag;
		UInt8 hasRandomAccessUnitsOnlyFlag, usePaddingFlag, useIdleFlag;
		UInt32 timeStampResolution, OCRResolution;
		UInt8 OCRLength;		// must be <= 64
		UInt8 AU_Length;		// must be <= 32
		UInt8 instantBitrateLength, degradationPriorityLength;
		UInt8 AU_seqNumLength; // must be <= 16
		UInt8 packetSeqNumLength; // must be <= 16
		UInt8 reserved; /* must be =0b11 */
		VALIDATE_FIELD("%d",  useAccessUnitStartFlag, 1);
		VALIDATE_FIELD("%d",  useAccessUnitEndFlag, 1);
		VALIDATE_FIELD("%d",  useRandomAccessPointFlag, 1);
		VALIDATE_FIELD("%d",  hasRandomAccessUnitsOnlyFlag, 1);
		VALIDATE_FIELD("%d",  usePaddingFlag, 1);
		VALIDATE_FIELD("%d",  useTimeStampsFlag, 1);
		VALIDATE_FIELD("%d",  useIdleFlag, 1);
		VALIDATE_FIELD("%d",  durationFlag, 1);
		
		VALIDATE_FIELD("%ld", timeStampResolution, 32);
		VALIDATE_FIELD("%ld", OCRResolution, 32);
		
		VALIDATE_FIELD("%d",  timeStampLength, 8);
		if (timeStampLength>64) errprint("Validate_SLconf_Descriptor: timeStampLength %d out of bounds\n",timeStampLength);
		VALIDATE_FIELD("%d",  OCRLength, 8);
		if (OCRLength>64) errprint("Validate_SLconf_Descriptor: OCRLength %d out of bounds\n",OCRLength);
		VALIDATE_FIELD("%d",  AU_Length, 8);
		if (AU_Length>64) errprint("Validate_SLconf_Descriptor: AU_Length %d out of bounds\n",AU_Length);
		VALIDATE_FIELD("%d",  instantBitrateLength, 8);
		
		VALIDATE_FIELD("%d",  degradationPriorityLength, 4);
		VALIDATE_FIELD("%d",  AU_seqNumLength, 5);
		if (AU_seqNumLength>16) errprint("Validate_SLconf_Descriptor: AU_seqNumLength %d out of bounds\n",AU_seqNumLength);
		VALIDATE_FIELD("%d",  packetSeqNumLength, 5);
		if (packetSeqNumLength>16) errprint("Validate_SLconf_Descriptor: packetSeqNumLength %d out of bounds\n",packetSeqNumLength);
		VALIDATE_FIELD_V("%d",  reserved, 2, 3, "Validate_SLconf_Descriptor");
	}
	else if (predefined == 1)
    {
        durationFlag = 0; useTimeStampsFlag = 0; timeStampLength = 32;
    }
    else
    {
        if (predefined != 2) {
            errprint("SLConfigDescriptor predefined %d is not recognized\n", predefined);
        }
        durationFlag = 0; useTimeStampsFlag = 1; timeStampLength = 0;
    }
	
	if (durationFlag) {
		UInt32 timeScale;
		UInt16 accessUnitDuration, compositionUnitDuration;
		VALIDATE_FIELD("%ld", timeScale, 32);
		VALIDATE_FIELD("%d",  accessUnitDuration, 16);
		VALIDATE_FIELD("%d",  compositionUnitDuration, 16);
	}
	if (!useTimeStampsFlag) {
		UInt32 startDecodingTimeStamp;
		UInt32 startCompositionTimeStamp;
		VALIDATE_FIELD("0x%lx", startDecodingTimeStamp, timeStampLength);
		VALIDATE_FIELD("0x%lx", startCompositionTimeStamp, timeStampLength);
	}
	
	--vg.tabcnt; atomprint("/>\n");
	
	SkipBytes(inbb,size);

bail:
	if (err) {
		errprint("Validate SLConfigDescriptor: %d\n",err);
	}
	return err;
}

//==========================================================================================

OSErr Validate_BIFSSpecificInfo(  BitBuffer *bb, UInt8 objectType )
{
	OSErr err;
	UInt8 use3DMeshCoding;
	UInt8 reserved;
	UInt8 nodeIDBits;
	UInt8 routeIDbits;
	UInt8 protoIDbits;
	UInt8 isCommandStream;
	UInt8 pixelMetric;
	UInt8 hasSize;
	UInt16 pixelWidth;
	UInt16 pixelHeight;
	UInt8 randomAccess;
	
	if (objectType == Object_Systems_1) {
		VALIDATE_FIELD("%d",  nodeIDBits, 5);
		VALIDATE_FIELD("%d",  routeIDbits, 5);
		VALIDATE_FIELD("%d",  isCommandStream, 1);
		if (isCommandStream==1) {
			VALIDATE_FIELD("%d",  pixelMetric, 1);
			VALIDATE_FIELD("%d",  hasSize, 1);
			if (hasSize==1) {
				VALIDATE_FIELD("%d",  pixelWidth, 16);
				VALIDATE_FIELD("%d",  pixelHeight, 16);
			}
		}
		else
		{
			VALIDATE_FIELD("%d",  randomAccess, 1);
			/* !dws anim mask goes here */
		}
	}
	else
	{
		VALIDATE_FIELD("%d",  use3DMeshCoding, 1);
		VALIDATE_FIELD("%d",  reserved, 1);
		VALIDATE_FIELD("%d",  nodeIDBits, 5);
		VALIDATE_FIELD("%d",  routeIDbits, 5);
		VALIDATE_FIELD("%d",  protoIDbits, 5);
		VALIDATE_FIELD("%d",  isCommandStream, 1);
		if (isCommandStream==1) {
			VALIDATE_FIELD("%d",  pixelMetric, 1);
			VALIDATE_FIELD("%d",  hasSize, 1);
			if (hasSize==1) {
				VALIDATE_FIELD("%d",  pixelWidth, 16);
				VALIDATE_FIELD("%d",  pixelHeight, 16);
			}
		}
		else
		{
			VALIDATE_FIELD("%d",  randomAccess, 1);
			/* !dws anim mask goes here */
		}
	}
	
	GetBits(bb,(bb->bits_left) & 7,nil);	/* align ourselves */
	
bail:
	if (err) {
		errprint("Validate_BIFSSpecificInfo: %d\n",err);
	}
	return err;
}

//==========================================================================================

OSErr Validate_SoundSpecificInfo(  BitBuffer *bb )
{
	OSErr err;
	UInt8 ext_audioObjectType = 0, audioObjectType, sbr_present = -1;
	UInt8 ext_samplingFreqIndex, samplingFreqIndex;
	UInt32 ext_samplingFreq, samplingFreq;
	UInt8 channelConfig;
	static char* audiotype[] = {
		"res", "AAC-Main", "AAC-LC", "AAC-SSR", "AAC-LTP", "GA-SBR", "AAC-scalable", "TwinVQ",
		"CELP", "HVXC", "res", "res", "TTS", "Main-synth", "WaveTbl-synth", "Gen-Midi" 
		"Alg-Synth-Audio-FX", "ER-AAC-LC", "res", "ER-AAC-LTP", "ER-AAC-scalable", 
		"ER-TwinVQ", "ER-BSAC", "ER-AAC-LD", "ER-CELP", "ER-HVXC", "ER-HILN", "ER-Parametric"
	};
	static char* freqs[] = {
		"96kHz", "88.2kHz", "64kHz", "48kHz", 
		"44.1kHz", "32kHz", "24kHz", "22.05kHz", 
		"16kHz", "12kHz", "11.025kHz", "8kHz",
		"7.35kHz", "reserved", "reserved", "escape" };
	static char* configs[] = {
		" ", "mono", "stereo", "stereo+center", "stereo+center+rearmono", "stereo+center+rearstereo",
		"5+1", "7+1" };
		
	VALIDATE_FIELD("0x%02x",  audioObjectType, 5);
	if (audioObjectType<28) {
		atomprint("  comment=\"audio is %s\"\n", audiotype[audioObjectType]);
	}
	VALIDATE_FIELD("0x%1x",  samplingFreqIndex, 4);
	if (samplingFreqIndex==0x0f) {
		VALIDATE_FIELD("%d",  samplingFreq, 24);
	}
	else atomprint("  comment=\"freq is %s\"\n", freqs[samplingFreqIndex]);
	
	VALIDATE_FIELD("0x%1x",  channelConfig, 4);
	if ((channelConfig>0) && (channelConfig<8)) {
		atomprint("  comment=\"config is %s\"\n", configs[channelConfig]);
	}
	
	if (audioObjectType == 5) {
		ext_audioObjectType = audioObjectType;
		sbr_present = 1;
		VALIDATE_FIELD("0x%1x",  ext_samplingFreqIndex, 4);
		if (ext_samplingFreqIndex==0x0f) {
			VALIDATE_FIELD("%d",  ext_samplingFreq, 24);
		}
		else atomprint("  comment=\"freq is %s\"\n", freqs[ext_samplingFreqIndex]);
		VALIDATE_FIELD("0x%02x",  audioObjectType, 5);
		if (audioObjectType<28) {
			atomprint("  comment=\"audio is %s\"\n", audiotype[audioObjectType]);
		}
	}

	switch (audioObjectType) {
		case 1: case 2: case 3: case 4: case 6: case 7:
		case 17: case 19: case 20: case 21: case 22: case 23:
			/* GA */
			{
				UInt8 frameLengthFlag;
				UInt8 dependsOnCoreCoder;
				UInt32 codeCoderdelay;
				UInt8 extFlag;
				VALIDATE_FIELD("%d",  frameLengthFlag, 1);
				atomprint("  comment=\"length is %d\"\n", (frameLengthFlag==0 ? 1024 : 960) ); /* !dws check with audio guys */
				VALIDATE_FIELD("%d",  dependsOnCoreCoder, 1);
				if (dependsOnCoreCoder==1) {
					VALIDATE_FIELD("%d",  codeCoderdelay, 14);
				}
				VALIDATE_FIELD("%d",  extFlag, 1);
				if (channelConfig==0) {
					UInt8 element_instance_tag, object_type;
					UInt8 num_front_channel_elements, num_side_channel_elements, num_back_channel_elements;
					UInt8 num_lfe_channel_elements, num_assoc_channel_elements, num_valid_cc_elements;
					UInt8 mono_mix_present, stereo_mix_present, matrix_mix_present;
					UInt8 is_cpe, tag_select, cc_element_is_ind_sw;
					UInt8 element_number, matrix_mixdown_idx, pseudo_surround_enable;
					UInt8 comment_field_bytes, i;
					char commentString[257] = {0};
					
					static char* aactypes[] = { "AAC Main", "AAC LC", "AAC SSR", "AAC LTP" };
					
					atomprint("<ProgramConfigElement\n"); vg.tabcnt++;
					
					/* atomprint("  comment=\"GA custom config here\"\n"); */
					VALIDATE_FIELD("%d",  element_instance_tag, 4);
					VALIDATE_FIELD("%d",  object_type, 2);
						atomprint("  comment=\"object is %s\"\n", aactypes[object_type]);
					VALIDATE_FIELD("0x%1x",  samplingFreqIndex, 4);
						atomprint("  comment=\"freq is %s\"\n", freqs[samplingFreqIndex]);

					VALIDATE_FIELD("%d",  num_front_channel_elements, 4);
					VALIDATE_FIELD("%d",  num_side_channel_elements, 4);
					VALIDATE_FIELD("%d",  num_back_channel_elements, 4);
					VALIDATE_FIELD("%d",  num_lfe_channel_elements, 2);
					VALIDATE_FIELD("%d",  num_assoc_channel_elements, 3);
					VALIDATE_FIELD("%d",  num_valid_cc_elements, 4);
					VALIDATE_FIELD("%d",  mono_mix_present, 1);
					if (mono_mix_present==1)
						VALIDATE_FIELD("%d",  element_number, 4);
					VALIDATE_FIELD("%d",  stereo_mix_present, 1);
					if (stereo_mix_present==1)
						VALIDATE_FIELD("%d",  element_number, 4);
					VALIDATE_FIELD("%d",  matrix_mix_present, 1);
					if (matrix_mix_present==1) {
						VALIDATE_FIELD("%d",  matrix_mixdown_idx, 2);
						VALIDATE_FIELD("%d",  pseudo_surround_enable, 1);
					}
					for (i=0; i<num_front_channel_elements; i++) {
						VALIDATE_INDEX_FIELD("%d",  is_cpe, 1, "front", i);
						VALIDATE_INDEX_FIELD("%d",  tag_select, 4, "front", i);
					}
					for (i=0; i<num_side_channel_elements; i++) {
						VALIDATE_INDEX_FIELD("%d",  is_cpe, 1, "side", i);
						VALIDATE_INDEX_FIELD("%d",  tag_select, 4, "side", i);
					}
					for (i=0; i<num_back_channel_elements; i++) {
						VALIDATE_INDEX_FIELD("%d",  is_cpe, 1, "back", i);
						VALIDATE_INDEX_FIELD("%d",  tag_select, 4, "back", i);
					}
					for (i=0; i<num_lfe_channel_elements; i++)
						VALIDATE_INDEX_FIELD("%d",  tag_select, 4, "lfe", i);
					for (i=0; i<num_assoc_channel_elements; i++)
						VALIDATE_INDEX_FIELD("%d",  tag_select, 4, "assoc", i);
					for (i=0; i<num_valid_cc_elements; i++) {
						VALIDATE_INDEX_FIELD("%d",  cc_element_is_ind_sw, 1, "cc", i);
						VALIDATE_INDEX_FIELD("%d",  tag_select, 4, "cc", i);
					}
					GetBits(bb,(bb->bits_left & 7),nil);	/* byte-align ourselves */
		
					BAILIFERRSET( comment_field_bytes = GetBits(bb, 8, &err) );
					if (comment_field_bytes>0) {
						BAILIFERR( GetBytes(bb, comment_field_bytes, (unsigned char*)commentString) );
						commentString[comment_field_bytes] = 0;
						atomprint("commentString =\"%s\"\n", commentString);
					}
				}
				if ((audioObjectType==6) | (audioObjectType==20)) {
					UInt8 layerNumber;
					VALIDATE_FIELD("%d",  layerNumber, 3);
				}
				if ( extFlag ) {
					UInt8 extensionFlag3;
					if ( audioObjectType == 22 ) {
						UInt16 numOfSubFrame, layer_length;
						VALIDATE_FIELD("%d",  numOfSubFrame, 5);
						VALIDATE_FIELD("%d",  layer_length, 11);
					}
					if ((audioObjectType == 17) || (audioObjectType == 18) ||
						(audioObjectType == 19) || (audioObjectType == 20) ||
						(audioObjectType == 21) || (audioObjectType == 23) ) {
						UInt8 aacScnDataResFlag, aacScfctrDataResFlag, aacSpctrlDataResFlag;
						VALIDATE_FIELD("%d",  aacScnDataResFlag, 1);
						VALIDATE_FIELD("%d",  aacScfctrDataResFlag, 1);
						VALIDATE_FIELD("%d",  aacSpctrlDataResFlag, 1);
					}
					VALIDATE_FIELD("%d",  extensionFlag3, 1);
					if ( extensionFlag3 ) {
						/* tbd in version 3 */
					}
				}
				
			}
			break;
		case 8:
			/* CELP */
			{ 
				UInt8 is_base_layer;
				UInt8 excit_Mode;
				UInt8 sampleRateMode;
				UInt8 fineRateControl;
				UInt8 rpe_conf;
				UInt8 mpe_conf;
				UInt8 numEnhLayers;
				UInt8 bandScalMode;
				VALIDATE_FIELD("%d",  is_base_layer, 1);
				if (is_base_layer == 1) {
					VALIDATE_FIELD("%d",  excit_Mode, 1);
					atomprint("  comment=\"mode is %s\"\n", (excit_Mode==0 ? "MPE" : "RPE") ); 
					VALIDATE_FIELD("%d",  sampleRateMode, 1);
					atomprint("  comment=\"mode is %s\"\n", (sampleRateMode==0 ? "8kHz" : "16kHz") );
					VALIDATE_FIELD("%d",  fineRateControl, 1);
					if (excit_Mode==1) /* RPE */ {
						VALIDATE_FIELD("%d",  rpe_conf, 3);
					}
					else { /* MPE */
						VALIDATE_FIELD("%d",  mpe_conf, 5);
						VALIDATE_FIELD("%d",  numEnhLayers, 2);
						VALIDATE_FIELD("%d",  bandScalMode, 1);
					}
				} 
				else
				{
					UInt8 isBWSLayer, BWSConf, BRS_id;
					VALIDATE_FIELD("%d",  isBWSLayer, 1);
					if (isBWSLayer == 1) 
						{ VALIDATE_FIELD("%d",  BWSConf, 2); } 
						else 
						{ VALIDATE_FIELD("%d",  BRS_id, 2); }
				}
				
			}
			break;
		case 9:
			/* HVXC */
			break;
		case 12:
			/* Text to speech */
			break;
		case 13: case 14: case 15:
			/* Structured audio */
			break;
	}
	
	if ((audioObjectType == 17) || (audioObjectType == 19) || 
		(audioObjectType == 20) || (audioObjectType == 21) ||
		(audioObjectType == 22) || (audioObjectType == 23) ||
		(audioObjectType == 24) || (audioObjectType == 25) ||
		(audioObjectType == 26) || (audioObjectType == 27) ) {
			UInt8 epConfig;
			VALIDATE_FIELD("%d",  epConfig, 2);
			if ( (epConfig == 2) || (epConfig == 3) ) 
			{	
				/* ErrorProtectionSpecificConfig()	*/
				UInt8 number_of_predef_set, interleave_type, bit_stuffing, num_of_concat_fr;
				UInt8 rs_fec_capability, header_protection;
				UInt32 i, j;
				VALIDATE_FIELD("%d",  number_of_predef_set, 8);
				VALIDATE_FIELD("%d",  interleave_type, 2);
				VALIDATE_FIELD("%d",  bit_stuffing, 3);
				VALIDATE_FIELD("%d",  num_of_concat_fr, 3);
				for ( i = 0; i < number_of_predef_set; i++ ) {	
					UInt8 num_of_class, class_reordered_output, class_output_order;
					VALIDATE_INDEX_FIELD1("%d",  num_of_class, 6, i);

					for ( j = 0; j < num_of_class; j++) {
						UInt8 length_escape, rate_escape, crclen_escape, concatenate_flag;
						UInt8 fec_type, termination_switch, interleave_switch, class_optional;
						UInt8 num_of_bits_for_len, class_rate, class_crclen;
						UInt16 class_length;
						VALIDATE_INDEX_FIELD1("%d",  length_escape, 1, j);
						VALIDATE_INDEX_FIELD1("%d",  rate_escape, 1, j);
						VALIDATE_INDEX_FIELD1("%d",  crclen_escape, 1, j);
						
						if ( num_of_concat_fr != 1) {	
							VALIDATE_INDEX_FIELD1("%d",  concatenate_flag, 1, j);
						}
							
						VALIDATE_INDEX_FIELD1("%d",  fec_type, 2, j);
						if( fec_type == 0) {	
							VALIDATE_INDEX_FIELD1("%d",  termination_switch, 1, j);
						}	
						if (interleave_type == 2) {	
							VALIDATE_INDEX_FIELD1("%d",  interleave_switch, 2, j);
						}	
						VALIDATE_INDEX_FIELD1("%d",  class_optional, 1, j);
						if ( length_escape == 1 ) {	/* ESC */	
							VALIDATE_INDEX_FIELD1("%d",  num_of_bits_for_len, 4, j);
						}	
						else {	
							VALIDATE_INDEX_FIELD1("%d",  class_length, 16, j);
						}	
						if ( rate_escape != 1 ) {	/* not ESC */	
							VALIDATE_INDEX_FIELD1("%d",  class_rate, 5, j);
						}	
						if ( crclen_escape != 1 ) { 	/* not ESC */	
							VALIDATE_INDEX_FIELD1("%d",  class_crclen, 5, j);
						}	
					}	
					VALIDATE_INDEX_FIELD1("%d",  class_reordered_output, 1, i);
					if ( class_reordered_output == 1 ) {	
						for ( j = 0; j < num_of_class; j++ ) {	
							VALIDATE_INDEX_FIELD1("%d",  class_output_order, 6, i);
						}	
					}	
				}	
				VALIDATE_FIELD("%d",  header_protection, 1);
				if ( header_protection == 1 ) {	
					UInt8 header_rate, header_crclen;
					VALIDATE_FIELD("%d",  header_rate, 5);
					VALIDATE_FIELD("%d",  header_crclen, 5);
				}	
				VALIDATE_FIELD("%d",  rs_fec_capability, 7);
			}
			if ( epConfig == 3 ) {
				UInt8 directMapping;
				VALIDATE_FIELD("%d",  directMapping, 1);
				if ( ! directMapping ) {
					/* tbd */
				}
			}
		}
	if ( (ext_audioObjectType != 5) && (bb->bits_left >= 16) ) {
		UInt16 syncExtensionType;
		VALIDATE_FIELD("0x%02x",  syncExtensionType, 11);
		if (syncExtensionType == 0x2b7) {
			VALIDATE_FIELD("0x%02x",  ext_audioObjectType, 5);
			if (ext_audioObjectType<28) {
				atomprint("  comment=\"audio is %s\"\n", audiotype[ext_audioObjectType]);
			}
			if ( ext_audioObjectType == 5 ) {
				VALIDATE_FIELD("%d",  sbr_present, 1);
				if (sbr_present == 1) {
					VALIDATE_FIELD("0x%1x",  ext_samplingFreqIndex, 4);
					if (ext_samplingFreqIndex==0x0f) {
						VALIDATE_FIELD("%d",  ext_samplingFreq, 24);
					}
					else atomprint("  comment=\"freq is %s\"\n", freqs[ext_samplingFreqIndex]);
				}
			}
		}
	}

	
	GetBits(bb,(bb->bits_left & 7),nil);	/* byte-align ourselves */
	
bail:
	if (err) {
		errprint("Validate_SoundSpecificInfo: %d\n",err);
	}
	return err;
}

//==========================================================================================

#define VALIDATE_FIELD_16S(fmt, var) do { \
	var = GetBits(bb, 16, &err); if (err) goto bail; \
	var = Endian16_Swap( var ); \
	atomprint("%s=\"" fmt "\"\n", #var, var); \
	} while (false)

#define VALIDATE_FIELD_32S(fmt, var) do { \
	var = GetBits(bb, 32, &err); if (err) goto bail; \
	var = Endian32_Swap( var ); \
	atomprint("%s=\"" fmt "\"\n", #var, var); \
	} while (false)



//==========================================================================================

UInt8 bitsizeof( UInt16 x )
{
	UInt8 i;
	i = 0;
	while (x!=0) { x = x>>1; i++; };
	return i;
}

OSErr ValidateQuantMat(BitBuffer *bb, char* matname)
{
	OSErr err;
	UInt8 load;
	
	load = GetBits(bb, 1, &err); if (err) goto bail;
	atomprint("load%smat=\"%d\"\n", matname, load);
	
	
	
	if (load==1) {
		UInt8 matValue;
		UInt16 count;
		count = 0;
		do { VALIDATE_FIELD("%d",  matValue, 8); count++; } while((count<64) && (matValue!=0));
	}
bail:
	if (err) {
		errprint("Validate %sMat: %d\n",matname,err);
	}
	return err;
}

static char* visual_profile_name(UInt8 indicator) {
	static char* thenames[] = {
		"SP@L1",    "SP@L2",    "SP@L3",    "SP@L0", "SSP@L0",
		"SSP@L1",   "SSP@L2",   "CP@L1",    "CP@L2",
		"MP@L2",    "MP@L3",    "MP@L4",    "NBP@L2",
		"STP@L1",   "SFAP@L1",  "SFAP@L2",  "SFBAP@L1",
		"SFBAP@L2", "BATP@L1",  "BATP@L2",  "AVC",
		"HP@L1",    "HP@L2",    "ARTSP@L1", "ARTSP@L2",
		"ARTSP@L3", "ARTSP@L4", "CSP@L1",   "CSP@L2", 
		"CSP@L3",   "ACEP@L1",  "ACEP@L2",  "ACEP@L3", 
		"ACEP@L4",  "ACP@L1",   "ACP@L2",   "AST@L1", 
		"AST@L2",   "AST@L3",   "S_STUDIO_P@L1", "S_STUDIO_P@L2", 
		"S_STUDIO_P@L3", "S_STUDIO_P@L4", "C_STUDIO_P@L1", "C_STUDIO_P@L2", 
		"C_STUDIO_P@L3", "C_STUDIO_P@L4",
		"ASP@L0",   "ASP@L1",  "ASP@L2",  "ASP@L3",
		"ASP@L4",   "ASP@L5",  "ASP@L3B", "FGSP@L0",
		"FGSP@L1",  "FGSP@L2", "FGSP@L3", "FGSP@L4",
		"FGSP@L5" };
	static UInt8 theindicators[] = {
		0x1,  0x2,  0x3,  0x8, 0x10,
		0x11, 0x12, 0x21, 0x22,
		0x32, 0x33, 0x34, 0x42,
		0x51, 0x61, 0x62, 0x63,
		0x64, 0x71, 0x72, 0x7f,
		0x81, 0x82, 0x91, 0x92,
		0x93, 0x94, 0xa1, 0xa2,
		0xa3, 0xb1, 0xb2, 0xb3,
		0xb4, 0xc1, 0xc2, 0xd1,
		0xd2, 0xd3, 0xe1, 0xe2,
		0xe3, 0xe4, 0xe5, 0xe6,
		0xe7, 0xe8,
		0xF0, 0xF1, 0xF2, 0xF3,
		0xF4, 0xF5, 0xF7, 0xf8,
		0xf9, 0xfa, 0xfb, 0xfc,
		0xfd
	};
	
	int i,j;
	
	i=sizeof(thenames)/sizeof(char*); j=sizeof(theindicators);
	
	if (i!=j) errprint("Internal program error (profile_name)\n");
	
	for (i=0; i<j; i++) {
		if (theindicators[i]==indicator) return thenames[i];
	}
	return "reserved";
}

OSErr Validate_VideoSpecificInfo(  BitBuffer *bb, UInt32 expect_startcode, UInt8 default_voVerID, void *p_sc )
{
	OSErr err;
	UInt32 startcode;
	UInt8 profileLevelInd, zeroBit;
	UInt8 voVerID;
	PartialVideoSC *p_vsc = p_sc;
	
	voVerID = default_voVerID;
	
	VALIDATE_FIELD("0x%04x",  startcode, 32);
	if ((expect_startcode!=0) && (startcode!=expect_startcode))
		errprint("Validate_VideoSpecificInfo: expected 0x%lx startcode, got 0x%lx\n",expect_startcode,startcode);
	
	if (startcode == VSC_VO_Sequence) {		
		profileLevelInd = GetBits(bb, 8, &err); if (err) goto bail;
		atomprint("profile=0x%02x profile_name=\"%s\"\n",profileLevelInd,visual_profile_name(profileLevelInd));
		/* VALIDATE_FIELD("0x%02x - \"(%s)\"",  profileLevelInd, 8); */
		p_vsc->profileLevelInd = profileLevelInd; // to check consistency of profile
		vg.visualProfileLevelIndication = profileLevelInd; // to check if corresponds to IOD

		startcode = 0;
		startcode = PeekBits(bb, 32, nil);
		while (startcode == VSC_UserData) {
			Validate_VideoSpecificInfo(bb,VSC_UserData,voVerID, p_sc );
			startcode = 0;
			startcode = PeekBits(bb, 32, nil);
		}
		
		err = Validate_VideoSpecificInfo(bb, VSC_VO, voVerID, p_sc);
		if (bb->bits_left>=32) {
			VALIDATE_FIELD_V("0x%04x",  startcode, 32, VSC_VO_Sequence_end, "VideoSpecificInfo");
		}
	}
	else if (startcode == VSC_UserData)
	{
		UInt32 count;
		count = 0;
		while (PeekBits(bb, 24, nil)!=1) {
			if (SkipBytes(bb,1)) break; count++;
		}
		atomprint("<VideoUserData length=\"%d\" />\n", count);
	}
	else if (startcode == VSC_VO) {
		UInt8 isVOID;
		UInt8 voPriority;
		UInt8 voType;
		VALIDATE_FIELD("%d",  isVOID, 1);
		if (isVOID==1) {
			voVerID = GetBits(bb, 4, &err); if (err) goto bail;
			atomprint("%s=%d\n", "voVerID", voVerID);
			if ((voVerID != 1) && (voVerID != 2) && (voVerID != 5))
				{ err = badAtomErr; errprint("Validate VideoSpecificInfo: voVerID != %d, should be 1, 2, or 5\n",voVerID); }
			/* VALIDATE_FIELD_V("%d",  voVerID, 4, 1, "VideoSpecificInfo"); */ /* all other values reserved */
			VALIDATE_FIELD  ("%d",  voPriority, 3);
		}
		VALIDATE_FIELD_V("%d",  voType, 4, 1, "VideoSpecificInfo"); /* else still/mesh/face/reserved */
		if ((voType==1) || (voType==2)) {
			UInt8 vSignalType;
			UInt8 vFormat;
			UInt8 vRange;
			UInt8 colourDesc;
			UInt8 colourPrimaries;
			UInt8 transferChars;
			UInt8 matrixCoeffs;
			VALIDATE_FIELD("%d",  vSignalType, 1);
			if (vSignalType==1) {
				static char* formats[] = {
					"Component", "PAL", "NTSC", "SECAM",
					"MAC", "Unspecified", "Reserved", "Reserved" };

				VALIDATE_FIELD("%d",  vFormat, 3);
				atomprint("  comment=\"format is %s\"\n",formats[vFormat]);
				VALIDATE_FIELD("%d",  vRange, 1);
				VALIDATE_FIELD("%d",  colourDesc, 1);
				if (colourDesc==1) {
					static char* primaries[] = {
						"Forbidden", "BT.709", "Unspecified", "Reserved","Linear/Film",
						"Log 100:1", "Log 316.22777:1"};
						
					VALIDATE_FIELD("%d",  colourPrimaries, 8);
					if (colourPrimaries<9) atomprint("  comment=\"primaries are %s\"\n",primaries[colourPrimaries]);
					VALIDATE_FIELD("%d",  transferChars, 8);
					if (transferChars<11) atomprint("  comment=\"transfer chars are %s\"\n",primaries[transferChars]);
					VALIDATE_FIELD("%d",  matrixCoeffs, 8);
				}
			}
		}
		GetBits(bb,(bb->bits_left & 7),nil);	/* byte-align ourselves */
		startcode = PeekBits(bb, 32, nil);
		if (startcode == VSC_UserData) Validate_VideoSpecificInfo(bb,VSC_UserData,voVerID, p_sc);

		if (voType==1) { /* video */
			startcode = GetBits(bb, 32, &err); if (err) goto bail;	/* a video object start code, 0 thru 1f */
			if ((startcode>=0x100) && (startcode <= 0x11F)) {
				OSErr err = noErr;
				startcode = PeekBits(bb,32,&err);
				if (err == outOfDataErr) {
					atomprint("  comment=\"short headers\"\n");
				}
				else if ((startcode >= 0x120) && (startcode <=  0x12F)) 
					{ err = Validate_VideoSpecificInfo(bb,0,voVerID, p_sc); }
				else errprint("Validate VideoSpecificInfo expected a VideoObjectLayerStartCode or nothing (short headers)\n");
			}
			else errprint("Validate VideoSpecificInfo expected a VideoObjectStartCode\n");
		}
		else if (voType==2) { /* still texture */
		}
		else if (voType==3) { /* mesh ID */
		}
		else if (voType==4) { /* face ID */
		}
	}
	else if ((startcode>=0x120) && (startcode <= 0x12F)) { /* The biggie - VOL */
		UInt8 randomAccessibleVol;
		UInt8 voTypeIndic;
		UInt8 isObjLayerId;
		UInt8 aspectRatioInfo;
		UInt8 volControlParams;
		UInt8 volShape;
		UInt16 vopTimeIncResolution;
		UInt8 marker;
		UInt8 fixedVopRate;
		UInt16 fixedVopTimeInc;
		UInt8 not8bit, quantType, complexEstimDis, resyncMarkerDisabled, dataPartioned, scalability;
		static char* vol_types[] = {
			"reserved", "simple", "simple scalable", "core", "main", "NBit",
			"Basic anim 2D", "anim 2D mesh", "simple face", "still scalable" };
		static char* ratios[] = {
			"Forbidden", "1:1", "12:11", "10:11", "16:11", "40:33" };
		static char* shapes[] = {
			"rectangular", "binary", "binary only", "grayscale" };
		
		VALIDATE_FIELD("%d",  randomAccessibleVol, 1);
		VALIDATE_FIELD("0x%02x",  voTypeIndic, 8);
		if (voTypeIndic<10) atomprint("  comment=\"type is %s\"\n",vol_types[voTypeIndic] );
		if (voTypeIndic == 0x12) {	/* "Fine Granularity Scalable" */
			UInt8 fgs_layer_type, vol_prio, fgs_ref_layer_id, quarter_sample, fgs_rs_mk_dis, interlaced;
			UInt16 volWidth, volHeight;
			
			VALIDATE_FIELD("0x%02x",  fgs_layer_type, 2);
			VALIDATE_FIELD("0x%02x",  vol_prio, 3);
			VALIDATE_FIELD("0x%1x",   aspectRatioInfo, 4);
			if (aspectRatioInfo==0xF) { /* extended_PAR */
				UInt8 parWidth;
				UInt8 parHeight;
				VALIDATE_FIELD("%d",  parWidth, 8);
				VALIDATE_FIELD("%d",  parHeight, 8);
			}
			else if (aspectRatioInfo<6) atomprint("  comment=\"aspect ratio is %s\"\n",ratios[aspectRatioInfo]);
			VALIDATE_FIELD("%d",  volControlParams, 1);
			if (volControlParams==1) {
				UInt8 chromaFormat;
				UInt8 lowDelay;
				VALIDATE_FIELD_V("%d",  chromaFormat, 2, 1, "VideoSpecificInfo"); /* 4:2:0, all other reserved */
				VALIDATE_FIELD("%d",  lowDelay, 1);
			}

			VALIDATE_FIELD_V("%d",  marker, 1, 1, "VideoSpecificInfo");
			VALIDATE_FIELD("%d",  vopTimeIncResolution, 16); VALIDATE_FIELD_V("%d",  marker, 1, 1, "VideoSpecificInfo");
			p_vsc->vopTimeIncResolution = vopTimeIncResolution;
			
			VALIDATE_FIELD("%d",  fixedVopRate, 1);
			if (fixedVopRate==1) {
				VALIDATE_FIELD("%d",  fixedVopTimeInc, bitsizeof(vopTimeIncResolution) );
			}
					
			VALIDATE_FIELD_V("%d",  marker, 1, 1, "VideoSpecificInfo");
			VALIDATE_FIELD("%d",  volWidth, 13);  VALIDATE_FIELD_V("%d",  marker, 1, 1, "VideoSpecificInfo");
			VALIDATE_FIELD("%d",  volHeight, 13); VALIDATE_FIELD_V("%d",  marker, 1, 1, "VideoSpecificInfo");
			p_vsc->volWidth = volWidth;
			p_vsc->volHeight = volHeight;
			
			VALIDATE_FIELD("%d",  interlaced, 1); 
			
			if (fgs_layer_type == 2 || fgs_layer_type == 3) /* "FGST" or "FGS_FGST" */
				VALIDATE_FIELD("0x%02x",  fgs_ref_layer_id, 4);

			if (fgs_layer_type == 1 || fgs_layer_type == 3) { /* "FGS" or "FGS_FGST" */
				UInt8 fgs_freq_wt_en;
				
				VALIDATE_FIELD("%d",  fgs_freq_wt_en, 1); 
				if (fgs_freq_wt_en) {
					UInt8 load;
					VALIDATE_FIELD("%d",  load, 1);
					if (load==1) {
						UInt8 mtxvalue;
						UInt16 count;
						count = 0;
						do { VALIDATE_FIELD("%d",  mtxvalue, 3); count++; } while((count<64) && (mtxvalue!=0));
					}
				}
			}
			if (fgs_layer_type == 2 || fgs_layer_type == 3) { /* "FGS" or "FGS_FGST" */
				UInt8 fgst_freq_wt_en;
				
				VALIDATE_FIELD("%d",  fgst_freq_wt_en, 1); 
				if (fgst_freq_wt_en) {
					UInt8 load;
					VALIDATE_FIELD("%d",  load, 1);
					if (load==1) {
						UInt8 mtxvalue;
						UInt16 count;
						count = 0;
						do { VALIDATE_FIELD("%d",  mtxvalue, 3); count++; } while((count<64) && (mtxvalue!=0));
					}
				}
			}
			VALIDATE_FIELD("%d",  quarter_sample, 1);
			VALIDATE_FIELD("%d",  fgs_rs_mk_dis, 1);
			GetBits(bb,(bb->bits_left & 7),nil);	/* align ourselves */
		} else {
			UInt8 volShapeExt;
			
			VALIDATE_FIELD("%d",  isObjLayerId, 1);
			if (isObjLayerId==1) {
				UInt8 voPriority;
				voVerID = GetBits(bb, 4, &err); if (err) goto bail;
				atomprint("%s=%d\n", "voVerID", voVerID);
				if ((voVerID != 1) && (voVerID != 2) && (voVerID != 5))
					{ err = badAtomErr; errprint("Validate VideoSpecificInfo: voVerID != %d, should be 1, 2, or 5\n",voVerID); }
				/* VALIDATE_FIELD_V("%d",  voVerID, 4, 1, "VideoSpecificInfo"); */ /* other values reserved */
				VALIDATE_FIELD  ("%d",  voPriority, 3);
			}
			else voVerID = default_voVerID;
			
			VALIDATE_FIELD("0x%1x",  aspectRatioInfo, 4);
			if (aspectRatioInfo==0xF) { /* extended_PAR */
				UInt8 parWidth;
				UInt8 parHeight;
				VALIDATE_FIELD("%d",  parWidth, 8);
				VALIDATE_FIELD("%d",  parHeight, 8);
			}
			else if (aspectRatioInfo<6) atomprint("  comment=\"aspect ratio is %s\"\n",ratios[aspectRatioInfo]);
			VALIDATE_FIELD("%d",  volControlParams, 1);
			if (volControlParams==1) {
				UInt8 chromaFormat;
				UInt8 lowDelay;
				UInt8 vbvParams;
				VALIDATE_FIELD_V("%d",  chromaFormat, 2, 1, "VideoSpecificInfo"); /* 4:2:0, all other reserved */
				VALIDATE_FIELD("%d",  lowDelay, 1);
				VALIDATE_FIELD("%d",  vbvParams, 1);
				if (vbvParams==1) {
					UInt16 firstHalfBitRate, latterHalfBitRate;
					UInt16 firstHalfVBVBuffer, latterHalfVBVBuffer;
					UInt16 firstHalfVBVOccup, latterHalfVBVOccup;
					VALIDATE_FIELD("%d",  firstHalfBitRate, 15);   VALIDATE_FIELD_V("%d",  marker, 1, 1, "VideoSpecificInfo");
					VALIDATE_FIELD("%d",  latterHalfBitRate, 15);  VALIDATE_FIELD_V("%d",  marker, 1, 1, "VideoSpecificInfo");
					VALIDATE_FIELD("%d",  firstHalfVBVBuffer, 15); VALIDATE_FIELD_V("%d",  marker, 1, 1, "VideoSpecificInfo");
					VALIDATE_FIELD("%d",  latterHalfVBVBuffer, 3);
					VALIDATE_FIELD("%d",  firstHalfVBVOccup, 11);  VALIDATE_FIELD_V("%d",  marker, 1, 1, "VideoSpecificInfo");
					VALIDATE_FIELD("%d",  latterHalfVBVOccup, 15); VALIDATE_FIELD_V("%d",  marker, 1, 1, "VideoSpecificInfo");
				}
			}
			VALIDATE_FIELD("%d",  volShape, 2); 
			atomprint("  comment=\"shape is %s\"\n",shapes[volShape]);
			if (volShape == 3 /* "grayscale" */
			    && voVerID != 1)
				VALIDATE_FIELD("%d",  volShapeExt, 4);
			
			VALIDATE_FIELD_V("%d",  marker, 1, 1, "VideoSpecificInfo");
			VALIDATE_FIELD("%d",  vopTimeIncResolution, 16); VALIDATE_FIELD_V("%d",  marker, 1, 1, "VideoSpecificInfo");
			VALIDATE_FIELD("%d",  fixedVopRate, 1);
			if (fixedVopRate==1) {
				VALIDATE_FIELD("%d",  fixedVopTimeInc, bitsizeof(vopTimeIncResolution) );
			}
			if (volShape!=2) { /* binary only */
				UInt8 interlaced;
				UInt8 obmcDisable;
				UInt8 spriteEnable;
				
				if (volShape==0) { /* rectangular */
					UInt16 volWidth, volHeight;
					VALIDATE_FIELD_V("%d",  marker, 1, 1, "VideoSpecificInfo");
					VALIDATE_FIELD("%d",  volWidth, 13);  VALIDATE_FIELD_V("%d",  marker, 1, 1, "VideoSpecificInfo");
					VALIDATE_FIELD("%d",  volHeight, 13); VALIDATE_FIELD_V("%d",  marker, 1, 1, "VideoSpecificInfo");
					p_vsc->volWidth = volWidth;
					p_vsc->volHeight = volHeight;
				}
				VALIDATE_FIELD("%d",  interlaced, 1); 
				VALIDATE_FIELD("%d",  obmcDisable, 1); 
				
				VALIDATE_FIELD("%d",  spriteEnable, (voVerID == 1 ? 1 : 2));
				if ((spriteEnable==1) || (spriteEnable==2)) {	/* static or GMC */
					UInt8 numSpriteWarpPts, spriteWarpAccuracy, spriteBrightChange, loLatencySpriteEnable;
					if (spriteEnable!=2) {
						UInt16 spriteWidth, spriteHeight, spriteLeft, spriteTop;
						VALIDATE_FIELD("%d",  spriteWidth, 13);  VALIDATE_FIELD_V("%d",  marker, 1, 1, "VideoSpecificInfo");
						VALIDATE_FIELD("%d",  spriteHeight, 13); VALIDATE_FIELD_V("%d",  marker, 1, 1, "VideoSpecificInfo");
						VALIDATE_FIELD("%d",  spriteLeft, 13);   VALIDATE_FIELD_V("%d",  marker, 1, 1, "VideoSpecificInfo");
						VALIDATE_FIELD("%d",  spriteTop, 13);    VALIDATE_FIELD_V("%d",  marker, 1, 1, "VideoSpecificInfo");
					}
					VALIDATE_FIELD("%d",  numSpriteWarpPts, 6);
					VALIDATE_FIELD("%d",  spriteWarpAccuracy, 2);
					VALIDATE_FIELD("%d",  spriteBrightChange, 1);
					if (spriteEnable!=2) VALIDATE_FIELD("%d",  loLatencySpriteEnable, 1);
				}
				if ((voVerID != 1) && (volShape != 0)) {
					UInt8 sadct_disable;
					VALIDATE_FIELD("%d",  sadct_disable, 1);
				}
				VALIDATE_FIELD("%d",  not8bit, 1);
				if (not8bit==1) {
					UInt8 quantPrecision, bitsPerPixel;
					VALIDATE_FIELD("%d",  quantPrecision, 4);
					VALIDATE_FIELD("%d",  bitsPerPixel, 4);
				}
				if (volShape==3) { /* grayscale */
					UInt8 noGrayQuantUpdate, compMethod, linearComp;
					VALIDATE_FIELD("%d",  noGrayQuantUpdate, 1);
					VALIDATE_FIELD("%d",  compMethod, 1);
					VALIDATE_FIELD("%d",  linearComp, 1);
				}
				VALIDATE_FIELD("%d",  quantType, 1);
				if (quantType==1) {
					err = ValidateQuantMat(bb,"IntraQuant"); if (err) goto bail;
					err = ValidateQuantMat(bb,"NonIntraQuant"); if (err) goto bail;
					if (volShape==3) { /* grayscale */
						err = ValidateQuantMat(bb,"IntraQuantGray"); if (err) goto bail;
						err = ValidateQuantMat(bb,"NonIntraQuantGray"); if (err) goto bail;
					}
				}
				if (voVerID != 1) {
					UInt8 quarter_sample;
					VALIDATE_FIELD("%d",  quarter_sample, 1);
				}
				VALIDATE_FIELD("%d",  complexEstimDis, 1);
				if (complexEstimDis==0) {
					UInt8 estimationMethod;
					VALIDATE_FIELD("%d",  estimationMethod, 2);
					if (estimationMethod==0) {
						UInt8 shapeComplexEstimDisable, textureComplexDisable1, textureComplexDisable2;
						UInt8 motionCompComplexDisable;
						VALIDATE_FIELD("%d",  shapeComplexEstimDisable, 1);
						if (shapeComplexEstimDisable==0) {
							UInt8 opaque, transparent, intraCAE, interCAE, noUpdate, upSampling;
							VALIDATE_FIELD("%d",  opaque, 1);
							VALIDATE_FIELD("%d",  transparent, 1);
							VALIDATE_FIELD("%d",  intraCAE, 1);
							VALIDATE_FIELD("%d",  interCAE, 1);
							VALIDATE_FIELD("%d",  noUpdate, 1);
							VALIDATE_FIELD("%d",  upSampling, 1);
						}
						VALIDATE_FIELD("%d",  textureComplexDisable1, 1);
						if (textureComplexDisable1==0) {
							UInt8 intraBlocks, interBlocks, inter4vBlocks, notCodedBlocks;
							VALIDATE_FIELD("%d",  intraBlocks, 1);
							VALIDATE_FIELD("%d",  interBlocks, 1);
							VALIDATE_FIELD("%d",  inter4vBlocks, 1);
							VALIDATE_FIELD("%d",  notCodedBlocks, 1);
						}
						VALIDATE_FIELD_V("%d",  marker, 1, 1, "VideoSpecificInfo");
						VALIDATE_FIELD("%d",  textureComplexDisable2, 1);
						if (textureComplexDisable2==0) {
							UInt8 dctCoeffs, dctLines, vlcSymbols, vlcBits;
							VALIDATE_FIELD("%d",  dctCoeffs, 1);
							VALIDATE_FIELD("%d",  dctLines, 1);
							VALIDATE_FIELD("%d",  vlcSymbols, 1);
							VALIDATE_FIELD("%d",  vlcBits, 1);
						}
						VALIDATE_FIELD("%d",  motionCompComplexDisable, 1);
						if (motionCompComplexDisable==0) {
							UInt8 apm, npm, interpolateMCQ, forwBackMCQ, halfPel2, halfPel4;
							VALIDATE_FIELD("%d",  apm, 1);
							VALIDATE_FIELD("%d",  npm, 1);
							VALIDATE_FIELD("%d",  interpolateMCQ, 1);
							VALIDATE_FIELD("%d",  forwBackMCQ, 1);
							VALIDATE_FIELD("%d",  halfPel2, 1);
							VALIDATE_FIELD("%d",  halfPel4, 1);
						}
						VALIDATE_FIELD_V("%d",  marker, 1, 1, "VideoSpecificInfo");
					}
				}
				VALIDATE_FIELD("%d",  resyncMarkerDisabled, 1);
				VALIDATE_FIELD("%d",  dataPartioned, 1);
				if (dataPartioned==1) {
					UInt8 reversibleVLC;
					VALIDATE_FIELD("%d",  reversibleVLC, 1);
				}
				if (voVerID != 1) {
					UInt8 newpred_enable, req_up_msg_t, np_seg_tp, red_res_vop_enable;
					VALIDATE_FIELD("%d",  newpred_enable, 1);
					if (newpred_enable) {
						VALIDATE_FIELD("%d",  req_up_msg_t, 2);
						VALIDATE_FIELD("%d",  np_seg_tp, 1);
					}
					VALIDATE_FIELD("%d",  red_res_vop_enable, 1);
				}
				VALIDATE_FIELD("%d",  scalability, 1);
				if (scalability==1) {
					UInt8 hierarchyType, refLayerID, refLayerSamplingDir;
					UInt8 horSamplingFactorN, horSamplingFactorM;
					UInt8 vertSamplingFactorN, vertSamplingFactorM, enhancementType;
					VALIDATE_FIELD("%d",  hierarchyType, 1);
					VALIDATE_FIELD("%d",  refLayerID, 4);
					VALIDATE_FIELD("%d",  refLayerSamplingDir, 1);
					VALIDATE_FIELD("%d",  horSamplingFactorN, 5);  VALIDATE_FIELD("%d",  horSamplingFactorM, 5);
					VALIDATE_FIELD("%d",  vertSamplingFactorN, 5); VALIDATE_FIELD("%d",  vertSamplingFactorM, 5);
					VALIDATE_FIELD("%d",  enhancementType, 1);
					if ((volShape == 1) && (hierarchyType == 0)) {
						UInt8 use_ref_shape, use_ref_texture;
						UInt8 shp_hz_sf_n, shp_hz_sf_m, shp_vt_sf_n, shp_vt_sf_m;
						VALIDATE_FIELD("%d",  use_ref_shape, 1);
						VALIDATE_FIELD("%d",  use_ref_texture, 1);
						
						VALIDATE_FIELD("%d",  shp_hz_sf_n, 5);
						VALIDATE_FIELD("%d",  shp_hz_sf_m, 5);
						VALIDATE_FIELD("%d",  shp_vt_sf_n, 5);
						VALIDATE_FIELD("%d",  shp_vt_sf_m, 5);
					}
				} 
			}
			else {
				if (voVerID != 1) {
					UInt8 scalability;
					VALIDATE_FIELD("%d",  scalability, 1);
					if (scalability) {
						UInt8 shp_hz_sf_n, shp_hz_sf_m, shp_vt_sf_n, shp_vt_sf_m;
						
						VALIDATE_FIELD("%d",  shp_hz_sf_n, 5);
						VALIDATE_FIELD("%d",  shp_hz_sf_m, 5);
						VALIDATE_FIELD("%d",  shp_vt_sf_n, 5);
						VALIDATE_FIELD("%d",  shp_vt_sf_m, 5);
					}
				}
				VALIDATE_FIELD("%d",  resyncMarkerDisabled, 1);
			}
			VALIDATE_FIELD("%d",  zeroBit, 1);		/* there is always at least one bit here */
			GetBits(bb,(bb->bits_left & 7),nil);	/* align ourselves */
		}
		startcode = 0;	/* in case the peek has no data */
		startcode = PeekBits(bb, 32, nil);
		while (startcode == VSC_UserData) {
			Validate_VideoSpecificInfo(bb,VSC_UserData, voVerID, p_sc);
			startcode = 0;	/* in case the peek has no data */
			startcode = PeekBits(bb, 32, nil);
		}
		

	}

bail:
	if (err) {
		errprint("Validate_VideoSpecificInfo: %d\n",err);
	}
	return err;
}

static UInt32 field_size(UInt32 x)
{
	UInt32 val = 0;
	while (x > 0) {
		val++;
		x = x>>1;
	}
	return val;
}

OSErr Validate_level_IDC( UInt8 profile, UInt8 level, UInt8 constraint_set3_flag)
{
	if ( ((profile == 66) || (profile == 77) || (profile == 88)) && 
		 (level==11) && 
		 (constraint_set3_flag==1)) 
	{ atomprint("  <Comment level 1b>\n"); } 
	else 
	{
		if (level>9) { 
			float x;
			x = level;
			x = x/10;
			atomprint("  <Comment level %3.1f>\n",x);
		}
		else { atomprint("  <Comment unknown level>\n"); }
	
		if ( ((profile == 100) || (profile == 110)  ) && (constraint_set3_flag==1))
		{ atomprint("  <Comment High 10 Intra profile compatible>\n"); }
		else if ( (profile == 122) && (constraint_set3_flag==1))
		{ atomprint("  <Comment High 4:2:2 Intra profile compatible>\n"); }
		else if (profile == 44) {
			if (constraint_set3_flag != 1) errprint("Error: constraint_set3_flag must be 1 when profile_idc is 44\n");
		}
		else if ( (profile == 244) && (constraint_set3_flag==1))
		{ atomprint("  <Comment High 4:4:4 Intra profile compatible>\n"); }

		else if (constraint_set3_flag == 1) 
				  warnprint("Warning: constraint_set3_flag==1 when it seems to be reserved to zero\n");
	}
}

OSErr Validate_AVCConfigRecord( BitBuffer *bb, void *refcon )
{
	TrackInfoRec *tir = (TrackInfoRec *)refcon;
	OSErr err;
	UInt32 i;
	//char	tempStr[100];
	UInt16 nal_length;
	AvcConfigInfo avcHeader;
	UInt8 reserved, constraint_set0_flag, constraint_set1_flag, constraint_set2_flag, constraint_set3_flag;
	UInt32* codec_specific;

	codec_specific = &((tir->validatedSampleDescriptionRefCons)[tir->currentSampleDescriptionIndex - 1]);

	avcHeader.config_ver 	= GetBits(bb, 8, &err); if (err) goto bail;
	avcHeader.profile 		= GetBits(bb, 8, &err); if (err) goto bail;

	atomprint("config=\"%d\"\n",avcHeader.config_ver);
	atomprint("profile=\"%d\"\n", avcHeader.profile);
	switch (avcHeader.profile) {
		case 66: atomprint("  <Comment %s profile>\n","baseline"); break;
		case 77: atomprint("  <Comment %s profile>\n","main"); break;
		case 88: atomprint("  <Comment %s profile>\n","extended"); break;
		case 100: atomprint("  <Comment %s profile>\n","high"); break;
		case 110: atomprint("  <Comment %s profile>\n","high 10"); break;
		case 122: atomprint("  <Comment %s profile>\n","high 4:2:2"); break;
		case 144: atomprint("  <Comment %s profile>\n","high 4:4:4"); break;
	}
	
	VALIDATE_FIELD  ("%d", constraint_set0_flag, 1);
		if ((avcHeader.profile==66) && (constraint_set0_flag==0))
			warnprint("Warning: Validate_AVCConfig: Baseline profile signalled but constraint_set0_flag not set\n",avcHeader.profile);
	VALIDATE_FIELD  ("%d", constraint_set1_flag, 1);
		if ((avcHeader.profile==77) && (constraint_set1_flag==0))
			warnprint("Warning: Validate_AVCConfig: Main profile signalled but constraint_set1_flag not set\n",avcHeader.profile);
	VALIDATE_FIELD  ("%d", constraint_set2_flag, 1);
	VALIDATE_FIELD  ("%d", constraint_set3_flag, 1);
	VALIDATE_FIELD_V("0x%02x", reserved, 4, 0, "Validate_AVCConfig");

	avcHeader.level 		= GetBits(bb, 8, &err); if (err) goto bail;
	atomprint("level=\"%d\"\n", avcHeader.level);
	
	Validate_level_IDC(avcHeader.profile, avcHeader.level, constraint_set3_flag);

	avcHeader.lengthsize = GetBits(bb, 8, &err); if (err) goto bail;

	if ((avcHeader.lengthsize & 0xFC) != 0xFC) {
		errprint( "reserved 1 bits are not 1 %x", avcHeader.lengthsize && 0xFC );
	}
	avcHeader.lengthsize = avcHeader.lengthsize & 3;
	codec_specific[0] = avcHeader.lengthsize + 1;
	
	atomprint("lengthsizeminusone=\"%d\"\n", avcHeader.lengthsize);
	atomprint("  <COMMENT \"length fields are %d bytes\"/>\n",avcHeader.lengthsize+1);
	if (avcHeader.lengthsize==2) errprint("AVC NALU lengths must be 1,2 or 4, not 3 bytes\n");
	
	avcHeader.sps_count = GetBits(bb, 8, &err); if (err) goto bail;
	if ((avcHeader.sps_count & 0xE0) != 0xE0) {
		errprint( "reserved 1 bits are not 1 %x", avcHeader.sps_count && 0xE0 );
	}
	avcHeader.sps_count = avcHeader.sps_count & 0x1F;
	atomprint("sps_count=\"%d\"\n", avcHeader.sps_count  );
	
	for (i=0; i< avcHeader.sps_count;  i++) {
		nal_length = GetBits(bb, 16, &err); if (err) goto bail;
		atomprint("nal_length=\"%d\"\n", nal_length  );

		/* then call the NALU validate proc */
		BAILIFERR( Validate_NAL_Unit( bb, nal_SPS, nal_length) );
		GetBits( bb, nal_length*8, & err);
	}    
	
	avcHeader.pps_count = GetBits(bb, 8, &err); if (err) goto bail;
	atomprint("pps_count=\"%d\"\n", avcHeader.pps_count );
	
	for (i=0; i< avcHeader.pps_count;  i++) {
		nal_length = GetBits(bb, 16, &err); if (err) goto bail;
		atomprint("nal_length=\"%d\"\n", nal_length  );
		
		/* then call the NALU validate proc */
		BAILIFERR( Validate_NAL_Unit( bb, nal_PPS, nal_length) );
		GetBits( bb, nal_length*8, & err);
	}  

	if ( (avcHeader.profile  ==  100)  ||  (avcHeader.profile  ==  110)  ||
		 (avcHeader.profile  ==  122)  ||  (avcHeader.profile  ==  144) ) {		
		VALIDATE_FIELD_V("%d", reserved, 6, 0x3F, "AVCConfigRecord");
		VALIDATE_FIELD("%d", avcHeader.chroma_format, 2);

		VALIDATE_FIELD_V("%d", reserved, 5, 0x1F, "AVCConfigRecord");
		VALIDATE_FIELD("%d", avcHeader.bit_depth_luma_minus8, 3);
		
		VALIDATE_FIELD_V("%d", reserved, 5, 0x1F, "AVCConfigRecord");
		VALIDATE_FIELD("%d", avcHeader.bit_depth_chroma_minus8, 3);

		avcHeader.sps_ext_count = GetBits(bb, 8, &err); if (err) goto bail;
		atomprint("sps_ext_count=\"%d\"\n", avcHeader.sps_ext_count );

		for (i=0; i< avcHeader.sps_ext_count;  i++) {
			nal_length = GetBits(bb, 16, &err); if (err) goto bail;
			atomprint("nal_length=\"%d\"\n", nal_length  );
			
			/* then call the NALU validate proc */
			BAILIFERR( Validate_NAL_Unit( bb, nal_SPS_Ext, nal_length) );
			GetBits( bb, nal_length*8, & err);
		}
	} 
	
	if (bb->bits_left != 0) errprint("Validate AVC Config record didn't use %ld bits\n", bb->bits_left);



bail:
	if (err) {
		errprint("Validate_AVCConfigRecord: %d\n",err);
	}
	return err;
}

static OSErr Validate_HRD( BitBuffer *bb )
{
	UInt32 cpb_cnt_minus1, SchedSelIdx, bit_rate_value_minus1, cpb_size_value_minus1;
	UInt8 bit_rate_scale, cpb_size_scale, cbr_flag;
	UInt8 initial_cpb_removal_delay_length_minus1, cpb_removal_delay_length_minus1;
	UInt8 dpb_output_delay_length_minus1, time_offset_length;
	
	OSErr err;
	
	VALIDATE_UEV( "0x%04x", cpb_cnt_minus1);
	
	VALIDATE_FIELD  ("0x%02x", bit_rate_scale, 4);
	VALIDATE_FIELD  ("0x%02x", cpb_size_scale, 4);
	for( SchedSelIdx = 0; SchedSelIdx <= cpb_cnt_minus1; SchedSelIdx++ ) {
		atomprint("<Schedule %d>\n",SchedSelIdx);
		
		VALIDATE_UEV(  "    0x%04x", bit_rate_value_minus1);
		VALIDATE_UEV(  "    0x%04x", cpb_size_value_minus1);
		VALIDATE_FIELD("    0x%02x", cbr_flag, 1);
		
		atomprint("</Schedule>\n",SchedSelIdx);
	}
	VALIDATE_FIELD("0x%02x", initial_cpb_removal_delay_length_minus1, 5);
	VALIDATE_FIELD("0x%02x", cpb_removal_delay_length_minus1, 5);
	VALIDATE_FIELD("0x%02x", dpb_output_delay_length_minus1, 5);
	VALIDATE_FIELD("0x%02x", time_offset_length, 5);

bail:
	if (err) {
		errprint("Validate_HRD: %d\n",err);
	}
	return err;
}


OSErr Validate_NAL_Unit(  BitBuffer *inbb, UInt8 expect_type, UInt32 nal_length )
{
	OSErr err;
	UInt8 zero_bit, nal_ref_idc, nal_type, one_bit;
	UInt32 trailing;
	BitBuffer mybb;
	BitBuffer *bb;
		static char* naltypes[] = {
		"Unspecified",									// 0 
		"Coded slice of a non-IDR picture", 			// 1
		"Coded slice data partition A",					// 2
		"Coded slice data partition B", 				// 3
		"Coded slice data partition C",					// 4
		"Coded slice of an IDR picture",				// 5
		"Supplemental enhancement information (SEI)", 	// 6
		"Sequence parameter set",						// 7
		"Picture parameter set",						// 8
		"Access unit delimiter",						// 9
		"End of sequence",								// 10
		"End of stream",								// 11
		"Filler data",									// 12
		"Sequence parameter set extension",				// 13
		"Prefix NAL Unit","Subset SPS","Reserved","Reserved","Reserved",	// 14-18
		"Coded slice of an auxiliary coded picture without partitioning",	// 19
		"Coded Slice Extension","Reserved","Reserved","Reserved",			// 20-23
		"Unspecified","Unspecified","Unspecified","Unspecified",
		"Unspecified","Unspecified","Unspecified","Unspecified"		// 24-31
		 };
	
	err = noErr;
	atomprint("<NALUnit length=%d (0x%x)\n",nal_length,nal_length); vg.tabcnt++;
	
	mybb = *inbb;
	mybb.bits_left = nal_length * 8;
	mybb.prevent_emulation = 1;
	bb = &mybb;

	/* strip the trailing bits so we can check for more_data at the end of PPSs, sigh */
	trailing = strip_trailing_zero_bits(bb, &err);

	if (trailing > 8) warnprint("Warning: Validate_NAL_Unit: more than 8 (%d) trailing bits\n", trailing);
	
	
	VALIDATE_FIELD_V("0x%01x", zero_bit, 1, 0, "NALUnit");
	VALIDATE_FIELD  ("0x%02x", nal_ref_idc, 2);
	if (expect_type) {
		VALIDATE_FIELD_V("0x%02x", nal_type, 5, expect_type, "NALUnit");
	} else {
		VALIDATE_FIELD  ("0x%02x", nal_type, 5);
	}
	atomprint("   <comment \"%s\"\\>\n",naltypes[nal_type]);
	
	switch (nal_type) {
		case nal_SPS:
		{
			UInt8 profile_idc, constraint_set0_flag, constraint_set1_flag, constraint_set2_flag, constraint_set3_flag;
			UInt8 reserved, level_idc, gaps_in_frame_num_value_allowed_flag, frame_mbs_only_flag, mb_adaptive_frame_field_flag;
			
			UInt32 seq_parameter_set_id, log2_max_frame_num_minus4, pic_order_cnt_type, log2_max_pic_order_cnt_lsb_minus4;
			UInt32 num_ref_frames, pic_width_in_mbs_minus1, pic_height_in_map_units_minus1;
			
			UInt8 direct_8x8_inference_flag, frame_cropping_flag, vui_parameters_present_flag;
			
			/* seq param set */
			
			VALIDATE_FIELD  ("%d", profile_idc, 8);
			switch (profile_idc) {
				case 66: atomprint("  <Comment %s profile>\n","baseline"); break;
				case 77: atomprint("  <Comment %s profile>\n","main"); break;
				case 88: atomprint("  <Comment %s profile>\n","extended"); break;
				case 100: atomprint("  <Comment %s profile>\n","high"); break;
				case 110: atomprint("  <Comment %s profile>\n","high 10"); break;
				case 122: atomprint("  <Comment %s profile>\n","high 4:2:2"); break;
				case 144: atomprint("  <Comment %s profile>\n","high 4:4:4"); break;
				default: warnprint("Warning: Validate_NAL_Unit (SPS): Unknown profile %d\n",profile_idc);
			}
			VALIDATE_FIELD  ("%d", constraint_set0_flag, 1);
				if ((profile_idc==66) && (constraint_set0_flag==0))
					warnprint("Warning: Validate_NAL_Unit (SPS): Baseline profile signalled but constraint_set0_flag not set\n",profile_idc);
			VALIDATE_FIELD  ("%d", constraint_set1_flag, 1);
				if ((profile_idc==77) && (constraint_set1_flag==0))
					warnprint("Warning: Validate_NAL_Unit (SPS): Main profile signalled but constraint_set1_flag not set\n",profile_idc);
			VALIDATE_FIELD  ("%d", constraint_set2_flag, 1);
			VALIDATE_FIELD  ("%d", constraint_set3_flag, 1);
			VALIDATE_FIELD_V("0x%02x", reserved, 4, 0, "Validate_NALU");
			VALIDATE_FIELD  ("%d", level_idc, 8);
			Validate_level_IDC( profile_idc, level_idc, constraint_set3_flag);
			
			VALIDATE_UEV( "0x%04x", seq_parameter_set_id);
			
			if( (profile_idc  ==  100)  ||  (profile_idc  ==  110)  ||
				(profile_idc  ==  122)  ||  (profile_idc  ==  144) ) {
				UInt32 chroma_format_idc, bit_depth_luma_minus8, bit_depth_chroma_minus8;
				UInt8 residual_colour_transform_flag, qpprime_y_zero_transform_bypass_flag, seq_scaling_matrix_present_flag;
				
				VALIDATE_UEV( "%d", chroma_format_idc);
				if( chroma_format_idc  ==  3 )		
					{ VALIDATE_FIELD  ("%d", residual_colour_transform_flag, 1); }
				VALIDATE_UEV( "%d", bit_depth_luma_minus8);
				VALIDATE_UEV( "%d", bit_depth_chroma_minus8);
				
				VALIDATE_FIELD  ("%d", qpprime_y_zero_transform_bypass_flag, 1);
				VALIDATE_FIELD  ("%d", seq_scaling_matrix_present_flag, 1);
				if( seq_scaling_matrix_present_flag ) {	
					UInt8 i, seq_scaling_list_present_flag;
					for( i = 0; i < 8; i++ ) {		
						VALIDATE_INDEX_FIELD1( "%d", seq_scaling_list_present_flag, 1, i);
						if( seq_scaling_list_present_flag )	{
							UInt8 sizeOfScalingList, j, useDefaultScalingMatrixFlag;
							UInt32 lastScale, nextScale;
							SInt32 delta_scale;
							
							if (i<6) sizeOfScalingList=16; else sizeOfScalingList=64;
							lastScale = 8;		
							nextScale = 8;
							for( j = 0; j < sizeOfScalingList; j++ ) {		
								if( nextScale != 0 ) {		
									VALIDATE_SEV( "%d", delta_scale);
									nextScale = ( lastScale + delta_scale + 256 ) % 256;	
									useDefaultScalingMatrixFlag = ( (j  ==  0) && (nextScale  ==  0) );
								}
								lastScale = ( nextScale  ==  0 ) ? lastScale : nextScale;
								atomprint("useDefaultScalingMatrixFlag[%d]=\"%d\"\n", j, useDefaultScalingMatrixFlag);
								atomprint("scalingList[%d]=\"%d\"\n", j, lastScale);		
							}		

						}
					}
				}
			}		

			VALIDATE_UEV( "%d", log2_max_frame_num_minus4);
			VALIDATE_UEV( "%d", pic_order_cnt_type);
				
			if( pic_order_cnt_type  ==  0 ) { VALIDATE_UEV( "0x%04x", log2_max_pic_order_cnt_lsb_minus4); }
			else if( pic_order_cnt_type  ==  1 ) {
				UInt32 i;	
				UInt32 delta_pic_order_always_zero_flag, num_ref_frames_in_pic_order_cnt_cycle;
				SInt32 offset_for_non_ref_pic, offset_for_top_to_bottom_field, offset_for_ref_frame;
					
				VALIDATE_FIELD  ("0x%02x", delta_pic_order_always_zero_flag, 1);
				VALIDATE_SEV( "0x%04x", offset_for_non_ref_pic);
				VALIDATE_SEV( "0x%04x", offset_for_top_to_bottom_field);
				VALIDATE_UEV( "0x%04x", num_ref_frames_in_pic_order_cnt_cycle);
				for( i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++ )		
					{ VALIDATE_SEV( "0x%04x", offset_for_ref_frame); }
			};
			VALIDATE_UEV    ( "%d", num_ref_frames);
			VALIDATE_FIELD  ("0x%01x", gaps_in_frame_num_value_allowed_flag, 1);
			VALIDATE_UEV    ( "%d", pic_width_in_mbs_minus1);
				atomprint("  <Comment \"width (PW + 1)*16 = %d\" \\>\n",(pic_width_in_mbs_minus1+1)*16);
			VALIDATE_UEV    ( "%d", pic_height_in_map_units_minus1);
				atomprint("  <Comment \"height (PH + 1)*16 = %d\" \\>\n",(pic_height_in_map_units_minus1+1)*16);
			
			VALIDATE_FIELD  ("%d", frame_mbs_only_flag, 1);
			
			if( !frame_mbs_only_flag )		
				{ VALIDATE_FIELD  ("0x%01x", mb_adaptive_frame_field_flag, 1); }
			VALIDATE_FIELD  ("0x%01x", direct_8x8_inference_flag, 1);
			VALIDATE_FIELD  ("0x%01x", frame_cropping_flag, 1);
			
			if( frame_cropping_flag ) {	
				UInt32 	frame_crop_left_offset, frame_crop_right_offset, frame_crop_top_offset, frame_crop_bottom_offset;
				VALIDATE_UEV    ( "%d", frame_crop_left_offset);
				VALIDATE_UEV    ( "%d", frame_crop_right_offset);
				VALIDATE_UEV    ( "%d", frame_crop_top_offset);
				VALIDATE_UEV    ( "%d", frame_crop_bottom_offset);
			};
			
			VALIDATE_FIELD  ("0x%01x", vui_parameters_present_flag, 1);
			if( vui_parameters_present_flag )		
			{ 
				UInt8 aspect_ratio_info_present_flag, overscan_info_present_flag, overscan_appropriate_flag;
				UInt8 video_signal_type_present_flag, chroma_loc_info_present_flag, timing_info_present_flag;
				UInt8 nal_hrd_parameters_present_flag, vcl_hrd_parameters_present_flag, pic_struct_present_flag;
				UInt8 bitstream_restriction_flag, low_delay_hrd_flag;
				
				VALIDATE_FIELD  ("0x%01x", aspect_ratio_info_present_flag, 1);
				if( aspect_ratio_info_present_flag ) {
					UInt8 aspect_ratio_idc;
					static char* aspect_types[] = {
						"unspecified", "1:1-square", "12:11", "10:11", "16:11", "40:33",
						"24:11", "20:11", "32:11", "80:33", "18:11", "15:11", "64:33", "160:99" };

					VALIDATE_FIELD  ("0x%01x", aspect_ratio_idc, 8);
					if( aspect_ratio_idc  ==  255 ) { /* Extended_SAR */
						UInt32 sar_width, sar_height;
						VALIDATE_FIELD  ("%d", sar_width,  16);
						VALIDATE_FIELD  ("%d", sar_height, 16);
					} else if (aspect_ratio_idc<=13)
					{
						atomprint("<Comment aspect ratio is %s>\n",aspect_types[aspect_ratio_idc]);
					}
				}
				VALIDATE_FIELD  ("0x%01x", overscan_info_present_flag, 1);
				if( overscan_info_present_flag )
					{ VALIDATE_FIELD  ("0x%01x", overscan_appropriate_flag, 1); }
				VALIDATE_FIELD  ("0x%01x", video_signal_type_present_flag, 1);
				if( video_signal_type_present_flag ) {
					UInt8 video_format, video_full_range_flag, colour_description_present_flag;
					UInt8 colour_primaries, transfer_characteristics, matrix_coefficients;
					static char* video_types[] = {
							"Component", "PAL", "NTSC", "SECAM", "MAC", "Unspecified" };
					static char* primaries[] = {
						"Reserved", "BT.709", "Unspecified", "Reserved","BT.470-2 System M",
						"BT.470-2 System B,G", "SMPTE 170M", "SMPTE 240M", "Linear/Film", 
						"Log 100:1", "Log 316.22777:1"};
					static char* matrices[] = {
						"Reserved", "BT.709", "Unspecified", "Reserved","FCC",
						"BT.470-2 System B,G", "SMPTE 170M", "SMPTE 240M" };

					VALIDATE_FIELD  ("0x%01x", video_format, 3);
					if (video_format<6) atomprint("<COMMENT \"video format is %s\"/>\n",video_types[video_format]);
					
					VALIDATE_FIELD  ("0x%01x", video_full_range_flag, 1);
					VALIDATE_FIELD  ("0x%01x", colour_description_present_flag, 1);
					if( colour_description_present_flag ) {
						VALIDATE_FIELD  ("0x%01x", colour_primaries, 8);
						if (colour_primaries<9) 
							atomprint("<COMMENT \"primaries are %s\"/>\n",primaries[colour_primaries]);
						VALIDATE_FIELD  ("0x%01x", transfer_characteristics, 8);
						if (transfer_characteristics<11) 
							atomprint("<COMMENT \"transfer characteristics are %s\"/>\n",primaries[transfer_characteristics]);
						VALIDATE_FIELD  ("0x%01x", matrix_coefficients, 8);
						if (matrix_coefficients<8) 
							atomprint("<COMMENT \"matrix coefficients are %s\"/>\n",matrices[matrix_coefficients]);
					}
				}
				VALIDATE_FIELD  ("0x%01x", chroma_loc_info_present_flag, 1);
				if( chroma_loc_info_present_flag ) {
					UInt8 chroma_sample_loc_type_top_field, chroma_sample_loc_type_bottom_field;
					
					VALIDATE_UEV    ( "%d", chroma_sample_loc_type_top_field);
					VALIDATE_UEV    ( "%d", chroma_sample_loc_type_bottom_field);
				}
				VALIDATE_FIELD  ("0x%01x", timing_info_present_flag, 1);
				if( timing_info_present_flag ) {
					UInt32 num_units_in_tick, time_scale, fixed_frame_rate_flag;
					
					VALIDATE_FIELD  ("%d", num_units_in_tick, 32);
					VALIDATE_FIELD  ("%d", time_scale, 32);
					VALIDATE_FIELD  ("0x%01x", fixed_frame_rate_flag, 1);
				}
				VALIDATE_FIELD  ("0x%01x", nal_hrd_parameters_present_flag, 1);
				if( nal_hrd_parameters_present_flag )
					{ BAILIFERR( Validate_HRD( bb )); /* hrd_parameters( ) */ }
				VALIDATE_FIELD  ("0x%01x", vcl_hrd_parameters_present_flag, 1);
				if( vcl_hrd_parameters_present_flag )
					{ BAILIFERR( Validate_HRD( bb )); /* hrd_parameters( ) */ }
				if( nal_hrd_parameters_present_flag  ||  vcl_hrd_parameters_present_flag )
					{ VALIDATE_FIELD  ("0x%01x", low_delay_hrd_flag, 1); }
				VALIDATE_FIELD  ("0x%01x", pic_struct_present_flag, 1); 
				VALIDATE_FIELD  ("0x%01x", bitstream_restriction_flag, 1);
				if( bitstream_restriction_flag ) {
					UInt8 motion_vectors_over_pic_boundaries_flag;
					UInt32 max_bytes_per_pic_denom, max_bits_per_mb_denom, log2_max_mv_length_horizontal;
					UInt32 log2_max_mv_length_vertical, num_reorder_frames, max_dec_frame_buffering;
					
					VALIDATE_FIELD  ("0x%01x", motion_vectors_over_pic_boundaries_flag, 1);
					VALIDATE_UEV    ( "%d", max_bytes_per_pic_denom);
					VALIDATE_UEV    ( "%d", max_bits_per_mb_denom);
					VALIDATE_UEV    ( "%d", log2_max_mv_length_horizontal);
					VALIDATE_UEV    ( "%d", log2_max_mv_length_vertical);
					VALIDATE_UEV    ( "%d", num_reorder_frames);
					VALIDATE_UEV    ( "%d", max_dec_frame_buffering);
				}

			 };	
					
			/* rbsp_trailing_bits( )	0 */	
			VALIDATE_FIELD_V("0x%01x",  one_bit, 1, 1, "NALUnit");
			zero_bit = GetBits(bb,(bb->bits_left & 7),nil);	/* we ought to be out of bits, whereupon this will return zero anyway */
			if (zero_bit != 0) errprint("\tTrailing zero bits not zero %d",zero_bit);

			}		

			break;
		case nal_PPS:
		{
			UInt32 pic_parameter_set_id, seq_parameter_set_id, num_slice_groups_minus1;
			UInt8 entropy_coding_mode_flag, pic_order_present_flag;
			
			UInt32 num_ref_idx_l0_active_minus1, num_ref_idx_l1_active_minus1;
			UInt8 weighted_pred_flag, weighted_bipred_idc, deblocking_filter_control_present_flag;
			UInt8 constrained_intra_pred_flag, redundant_pic_cnt_present_flag;
			SInt32 pic_init_qp_minus26, pic_init_qs_minus26, chroma_qp_index_offset;
			
			VALIDATE_UEV( "0x%04x", pic_parameter_set_id);
			VALIDATE_UEV( "0x%04x", seq_parameter_set_id);
			
			VALIDATE_FIELD  ("0x%01x", entropy_coding_mode_flag, 1);
			VALIDATE_FIELD  ("0x%01x", pic_order_present_flag, 1);
			VALIDATE_UEV( "0x%04x", num_slice_groups_minus1);


			if( num_slice_groups_minus1 > 0 ) {	
				UInt32 slice_group_map_type, iGroup;
				
				VALIDATE_UEV( "0x%04x", slice_group_map_type);
				if( slice_group_map_type  ==  0 )	{
					UInt32 run_length_minus1;
					
					for( iGroup = 0; iGroup <= num_slice_groups_minus1; iGroup++ )		
						{ VALIDATE_UEV( "0x%04x", run_length_minus1); }
				}
				else if( slice_group_map_type  ==  2 ) {		
					UInt32 top_left, bottom_right;
					for( iGroup = 0; iGroup < num_slice_groups_minus1; iGroup++ ) {		
						VALIDATE_UEV( "0x%04x", top_left);
						VALIDATE_UEV( "0x%04x", bottom_right);
					}
				}	
				else if(  	(slice_group_map_type  ==  3)  ||  
							(slice_group_map_type  ==  4)  ||  
							(slice_group_map_type  ==  5) ) {
					UInt8 slice_group_change_direction_flag;
					UInt32 slice_group_change_rate_minus1;
					VALIDATE_FIELD  ("0x%01x", slice_group_change_direction_flag, 1);
					VALIDATE_UEV( "0x%04x", slice_group_change_rate_minus1);
				} else if( slice_group_map_type  ==  6 ) {	
					UInt32 	pic_size_in_map_units_minus1, i;
					VALIDATE_UEV( "0x%04x", pic_size_in_map_units_minus1);
					for( i = 0; i <= pic_size_in_map_units_minus1; i++ )		
					{ 
						UInt32 slice_group_id;
						VALIDATE_FIELD  ("0x%04x", slice_group_id, field_size(num_slice_groups_minus1));
					};
				}		
			}
			VALIDATE_UEV( "%d", num_ref_idx_l0_active_minus1);
			VALIDATE_UEV( "%d", num_ref_idx_l1_active_minus1);
			VALIDATE_FIELD  ("0x%01x", weighted_pred_flag, 1);
			VALIDATE_FIELD  ("0x%01x", weighted_bipred_idc, 2);
			
			VALIDATE_SEV( "%d", pic_init_qp_minus26);
			VALIDATE_SEV( "%d", pic_init_qs_minus26);
			VALIDATE_SEV( "%d", chroma_qp_index_offset);
			
			VALIDATE_FIELD  ("0x%01x", deblocking_filter_control_present_flag, 1);
			VALIDATE_FIELD  ("0x%01x", constrained_intra_pred_flag, 1);
			VALIDATE_FIELD  ("0x%01x", redundant_pic_cnt_present_flag, 1);
			
			if (mybb.bits_left > 1) {
				UInt8 transform_8x8_mode_flag, pic_scaling_matrix_present_flag;
				SInt32 second_chroma_qp_index_offset;
				VALIDATE_FIELD  ("0x%01x", transform_8x8_mode_flag, 1);
				VALIDATE_FIELD  ("0x%01x", pic_scaling_matrix_present_flag, 1);
				if ( pic_scaling_matrix_present_flag ) {	
					UInt8 i, pic_scaling_list_present_flag;
					for( i = 0; i < 8; i++ ) {		
						VALIDATE_INDEX_FIELD1( "%d", pic_scaling_list_present_flag, 1, i);
						if( pic_scaling_list_present_flag )	{
							UInt8 sizeOfScalingList, j, useDefaultScalingMatrixFlag;
							UInt32 lastScale, nextScale;
							SInt32 delta_scale;
							
							if (i<6) sizeOfScalingList=16; else sizeOfScalingList=64;
							lastScale = 8;		
							nextScale = 8;
							for( j = 0; j < sizeOfScalingList; j++ ) {		
								if( nextScale != 0 ) {		
									VALIDATE_SEV( "%d", delta_scale);
									nextScale = ( lastScale + delta_scale + 256 ) % 256;	
									useDefaultScalingMatrixFlag = ( (j  ==  0) && (nextScale  ==  0) );
								}
								lastScale = ( nextScale  ==  0 ) ? lastScale : nextScale;
								atomprint("useDefaultScalingMatrixFlag[%d]=\"%d\"\n", j, useDefaultScalingMatrixFlag);
								atomprint("scalingList[%d]=\"%d\"\n", j, lastScale);		
							}		

						}
					}
				}
				VALIDATE_SEV( "%d", second_chroma_qp_index_offset);
			}
			
			VALIDATE_FIELD_V("0x%01x",  one_bit, 1, 1, "NALUnit");
			zero_bit = GetBits(bb,(bb->bits_left & 7),nil);	/* we ought to be out of bits, whereupon this will return zero anyway */
			if (zero_bit != 0) errprint("\tTrailing zero bits not zero %d",zero_bit);

		}
			break;

		case nal_SPS_Ext:
		{
			UInt32 seq_parameter_set_id, aux_format_idc;
			UInt8 additional_extension_flag;
			
			VALIDATE_UEV( "0x%04x", seq_parameter_set_id);
			VALIDATE_UEV( "%d", aux_format_idc);
			if( aux_format_idc  !=  0 ) {
				UInt32 bit_depth_aux_minus8, alpha_opaque_value, alpha_transparent_value;
				UInt8 alpha_incr_flag;
				
				VALIDATE_UEV( "%d", bit_depth_aux_minus8);
				VALIDATE_FIELD( "%d", alpha_incr_flag, 1);
				VALIDATE_FIELD( "%d", alpha_opaque_value,  bit_depth_aux_minus8 + 9);
				VALIDATE_FIELD( "%d", alpha_transparent_value,  bit_depth_aux_minus8 + 9);
			}		
			VALIDATE_FIELD("%d", additional_extension_flag, 1);
			
			VALIDATE_FIELD_V("0x%01x",  one_bit, 1, 1, "NALUnit");
			zero_bit = GetBits(bb,(bb->bits_left & 7),nil);	/* we ought to be out of bits, whereupon this will return zero anyway */
			if (zero_bit != 0) errprint("\tTrailing zero bits not zero %d",zero_bit);
		}
		break;
		
		/* case nal_slice_data_partition_a_layer:
		case nal_slice_layer_without_partitioning:
		case nal_slice_layer_without_partitioning_IDR: */
		case 255:
		/* could be for nal_slice_data_partition_a_layer, nal_slice_layer_without_partitioning and nal_slice_layer_without_partitioning_IDR */
		/* but there are parameters to get from the SPS, alas */
		{
			static char* slice_types[] = { "P (P slice)", "B (B slice)", "I (I slice)", "SP (SP slice)", "SI (SI slice)",
										   "P (P slice)", "B (B slice)", "I (I slice)", "SP (SP slice)", "SI (SI slice)" };
			UInt32 first_mb_in_slice, slice_type, pic_parameter_set_id, frame_num;
			
			VALIDATE_UEV( "%d", first_mb_in_slice);
			VALIDATE_UEV( "%d", slice_type);
				if (slice_type<10) atomprint("<COMMENT \"slice type is %s\"/>\n",slice_types[slice_type]);
			VALIDATE_UEV( "%d", pic_parameter_set_id);
			/* now we have to find log2_max_frame_num_minus4, and frame_mbs only from the SPS linked to the PPS of this ID.  ugh */
			VALIDATE_FIELD( "%d", frame_num, 7);		/* 7 == #bits from SPS log2_max_frame_num_minus4 + 4 */
			
			if (1) {	/* should be !frame_mbs_only from SPS */
				UInt8 field_pic_flag, bottom_field_flag;
				VALIDATE_FIELD( "%d", field_pic_flag, 1);
				if (field_pic_flag) { VALIDATE_FIELD( "%d", bottom_field_flag, 1); }
			}
			if (nal_type == nal_slice_layer_without_partitioning_IDR) {
				UInt32 idr_pic_id;
				VALIDATE_UEV( "%d", idr_pic_id);
			}
		}
		break;
		
		default:
			/* sampleprinthexdata((void*)bb->cptr, nal_length); */
			// SkipBytes(bb,nal_length-1);
			{
				unsigned int i;
				i = bb->bits_left;
				while (i>=8) {
					GetBits(bb, 8, &err); i-=8;
					if (err) break;
				}
				if (i>0) GetBits(bb, i, &err);
			}
			// errprint("\tUnknown NAL Unit %d",nal_type);
			break;
	}
	if (bb->bits_left != 0) errprint("Validate NAL Unit didn't use %ld bits\n", bb->bits_left);

bail:
	--vg.tabcnt; atomprint("/>\n");

	if (err) {
		errprint("Validate_NalUnit: %d\n",err);
	}
	return err;
}


//==========================================================================================

OSErr Validate_DecSpecific_Descriptor( BitBuffer *inbb, UInt8 ObjectType, UInt8 StreamType, void *p_sc )
{
	OSErr err = noErr;
	BitBuffer newbb;
	BitBuffer *bb;
	
	UInt32 tag;
	UInt32 size;

	atomprint("<DecoderSpecificInfo"); vg.tabcnt++;
	bb = inbb;

	if (bb->bits_left % 8) {
		errprint("Validate DecoderSpecificInfo did not start byte aligned\n");
	}

	BAILIFERR( GetDescriptorTagAndSize(bb, &tag, &size) );
	atomprintnotab("\ttag=\"0x%2.2x\" size=\"%d\"\n", tag, size);
	FieldMustBe( tag, Class_DecSpecificInfoTag, "Validate_DecSpecific_Descriptor: Tag != Class_DecSpecificInfoTag\n" );
	
	newbb = *inbb;
	bb = &newbb;
	
	bb->bits_left = size * 8;
		
	switch (ObjectType) {
		case Object_Systems_1:
		case Object_Systems_2:
			if (StreamType==Stream_BIFS)
				err = Validate_BIFSSpecificInfo( bb, ObjectType );
			break;
		
		case Object_Unspecified:		/* some old systems streams use this tag -- dws */
			if (StreamType==Stream_BIFS)
				err = Validate_BIFSSpecificInfo( bb, Object_Systems_1 );
			break;
			
		case Object_Audio_14496:
			err = Validate_SoundSpecificInfo( bb );
			break;
			
		case Object_Visual_14496:
			err = Validate_VideoSpecificInfo(  bb, VSC_VO_Sequence, 1, (void*)p_sc);		/* The overall default voVerID is 1 */
			break;
		
		case Object_Visual_AVC:
			err = Validate_AVCConfigRecord( bb, NULL );
			break;
		
			
		default:
			break;
	}
	
	if (bb->bits_left != 0) errprint("Validate DecoderSpecificInfo didn't use %ld bits\n", bb->bits_left);

	err = SkipBytes(inbb, size); if (err) goto bail;
	
	--vg.tabcnt; atomprint("/>\n");

bail:
	if (err) {
		errprint("Validate_DecSpecific_Descriptor: %d\n",err);
	}
	return err;
}

//==========================================================================================

OSErr Validate_Random_Descriptor(BitBuffer *bb, char* dname)
{
	OSErr err = noErr;
	UInt32 tag;
	UInt32 size;
	char* tagname = "Unknown";
	
	static char* tags1[] = {
		"Forbidden", 				// 0 
		"ObjectDescrTag", 			// 1
		"InitialObjectDescrTag", 	// 2
		"ES_DescrTag", 				// 3
		"DecoderConfigDescrTag",	// 4
		"DecSpecificInfoTag", 		// 5
		"SLConfigDescrTag", 		// 6
		"ContentIdentDescrTag", 	// 7
		"SupplContentIdentDescrTag",	// 8
		"IPI_DescrPointerTag",		// 9
		"IPMP_DescrPointerTag",		// 0a
		"IPMP_DescrTag",			// 0b
		"QoS_DescrTag",				// 0c
		"RegistrationDescrTag",		// 0d
		"ES_ID_IncTag",				// 0e
		"ES_ID_RefTag",				// 0f
		"MP4_IOD_Tag",				// 10
		"MP4_OD_Tag",				// 11
		"IPI_DescrPointerRefTag",	// 12
		"ExtendedProfileLevelDescrTag",	// 13
		"profileLevelIndicationIndexDescrTag" };	// 14
	static char* tags2[] = {
		"ContentClassificationDescrTag", 		// 40
		"KeyWordDescrTag",					// 41
		"RatingDescrTag",					// 42
		"LanguageDescrTag",					// 43
		"ShortTextualDescrTag",				// 44
		"ExpandedTextualDescrTag",			// 45
		"ContentCreatorNameDescrTag",		// 46
		"ContentCreationDateDescrTag",		// 47
		"OCICreatorNameDescrTag",			// 48
		"OCICreationDateDescrTag",			// 49
		"SmpteCameraPositionDescrTag" };	// 4a
		
	tag = PeekBits(bb, 8, &err); if (err) goto bail;
	switch (tag) {
		case Class_ObjectDescrTag:
		case Class_MP4_OD_Tag:
			err = Validate_Object_Descriptor(bb);
			break;
		case Class_ES_DescrTag:
			err = Validate_ES_Descriptor(bb, 0, 0, false);
			break;
		case Class_DecoderConfigDescTag:
			err = Validate_Dec_conf_Descriptor(bb, 0, 0);
			break;
		case Class_SLConfigDescrTag:
			err = Validate_SLconf_Descriptor(bb, false);
			break;
		case Class_ES_ID_IncTag:
			err = Validate_ES_INC_Descriptor(bb);
			break;
		case Class_ES_ID_RefTag:
			err = Validate_ES_REF_Descriptor(bb);
			break;
		case 5:
			//err = Validate_DecSpecific_Descriptor( BitBuffer *bb, UInt8 ObjectType, UInt8 StreamType, void *p_sc );
			// should never occur in practice;  we should only find one where we expect it,
			// so drop through to default, as we don't have the parameters needed
		default:
			atomprint("<%s", dname); vg.tabcnt++;

			if (bb->bits_left % 8) {
				errprint("Validate %s did not start byte aligned\n",dname);
			}

			BAILIFERR( GetDescriptorTagAndSize(bb, &tag, &size) );
			
			if ((tag == 0) || (tag == 0xFF)) tagname = tags1[0];
			else if (tag <= 0x14) tagname = tags1[tag];
			else if (tag < 0x40) tagname = "ISO Reserved";
			else if (tag <= 0x4a) tagname = tags2[ tag - 0x40 ];
			else if (tag <= 0x5F) tagname = "ISO OCI Reserved";
			else if (tag <= 0xBF) tagname = "ISO Reserved";
			else if (tag <= 0xFE) tagname = "user private";
			else tagname = "Forbidden";
			
			atomprintnotab("\ttag=\"0x%2.2x\" size=\"%d\">\n", tag, size);
			atomprint("<comment descriptor tag is %s />\n",tagname);
			
			atomprinthexdata((void*)bb->cptr, size);
			atomprint("/>\n");
			
			
			err = SkipBytes(bb, size); if (err) goto bail;

			--vg.tabcnt; atomprint("</%s>\n", dname);
			break;
	}

bail:
	if (err) {
		errprint("Validate %s: %d\n",dname,err);
	}
	return err;
}


//==========================================================================================

OSErr Validate_IPMP_DescriptorPointer(BitBuffer *bb)
{
	return Validate_Random_Descriptor( bb, "IPMP_descriptor");
}

//==========================================================================================

OSErr Validate_Extension_DescriptorPointer(BitBuffer *bb)
{
	return Validate_Random_Descriptor( bb, "Extension_Descriptor");
}

//==========================================================================================

OSErr Validate_soun_ES_Bitstream( BitBuffer *bb, void *refcon )
{
	TrackInfoRec *tir = (TrackInfoRec *)refcon;
	OSErr err;

	atomprint("\n");
	err = Validate_ES_Descriptor( bb, Object_Audio_14496, Stream_Audio, true );  if (err) goto bail;
				
bail:
	if (err) {
		errprint("Validate_soun_ES_Bitstream: %d\n",err);
	}
	return err;
}


//==========================================================================================

OSErr Validate_vide_ES_Bitstream( BitBuffer *bb, void *refcon )
{
	TrackInfoRec *tir = (TrackInfoRec *)refcon;
	OSErr err;

	atomprint("\n");
	err = Validate_ES_Descriptor( bb, Object_Visual_14496, Stream_Visual, true );  if (err) goto bail;
				
	// Stash special info for later
	// !dws should this be done in all ES validators, or just video??
	tir->validatedSampleDescriptionRefCons[0] = 5;
//    for global refcon use [0]
	
bail:
	if (err) {
		errprint("Validate_vide_ES_Bitstream: %d\n",err);
	}
	return err;
}


//==========================================================================================

OSErr Validate_mp4s_ES_Bitstream( BitBuffer *bb, void *refcon )
{
#pragma unused(refcon)
	OSErr err;

	atomprint("\n");
	err = Validate_ES_Descriptor( bb, 0, 0, true );  if (err) goto bail;
		/* we don't know what stream type or object type ... dws */
				
bail:
	if (err) {
		errprint("Validate_mp4s_ES_Bitstream: %d\n",err);
	}
	return err;
}

//==========================================================================================

OSErr Validate_sdsm_ES_Bitstream( BitBuffer *bb, void *refcon )
{
#pragma unused(refcon)
	OSErr err;

	atomprint("\n");
	err = Validate_ES_Descriptor( bb, 0, Stream_BIFS, true );  if (err) goto bail;
		/* the object type might be 1, 2, or 0xFE ... dws */
				
bail:
	if (err) {
		errprint("Validate_sdsm_ES_Bitstream: %d\n",err);
	}
	return err;
}

//==========================================================================================

OSErr Validate_odsm_ES_Bitstream( BitBuffer *bb, void *refcon )
{
#pragma unused(refcon)
	OSErr err;

	atomprint("\n");
	err = Validate_ES_Descriptor( bb, 0, Stream_OD, true );  if (err) goto bail;
		/* the object type might be 1, 2, or 0xFE ... dws */
				
bail:
	if (err) {
		errprint("Validate_odsm_ES_Bitstream: %d\n",err);
	}
	return err;
}

//==========================================================================================

OSErr Validate_sdsm_sample_Bitstream( BitBuffer *bb, void *refcon )
{
#pragma unused(refcon)
	OSErr err;
	UInt32 command;
	static char* commands[] = {
				"Insertion", "Deletion", "Replacement", "SceneReplacement" };


	
	command = PeekBits( bb, 2, &err );
	if (err) goto bail;
	sampleprint("<comment BIFS command is %s />\n",commands[command]);
	
	sampleprinthexdata((void*)bb->cptr, bb->length);

	err = SkipBytes(bb, bb->length); if (err) goto bail;

	if (NumBytesLeft(bb) > 1) {
		err = tooMuchDataErr;
	}
	
bail:
	if (err) {
		errprint("Validate_sdsm_sample_Bitstream: %d\n",err);
	}
	return err;
}


//==========================================================================================

OSErr Validate_odsm_sample_Bitstream( BitBuffer *bb, void *refcon )
{
#pragma unused(refcon)
	OSErr err;

	UInt32 esTag;
	UInt32 esSize;
	UInt16 OD_ID, ES_ID, IPMP_DescrID, Track_ref_Index;
	
	UInt8 printed = 0;
	Boolean saved_printatom;
	
	static char* commands[] = {
		"forbidden", "ODUpdate", "ODRemove", "ESUpdate", "ESRemove",
		"IPMPUpdate", "IPMPRemove", "ESRemoveRef", "ODExec" };


	while (bb->bits_left > 0) {
		BAILIFERR( GetDescriptorTagAndSize(bb, &esTag, &esSize) );
		sampleprint("<OD_sample tag=\"0x%2.2x\" size=\"%d\" >\n", esTag, esSize); vg.tabcnt++;
				
		if (esTag==0) errprint("Validate_odsm_sample_Bitstream: OD Stream uses forbidden command %x\n",esTag);
		else if (esTag<=8) 
		{
			UInt32 bits_to_leave;
			bits_to_leave = bb->bits_left - (esSize*8); if (bits_to_leave<0) bits_to_leave = 0;
			sampleprint("<comment command is %s />\n",commands[esTag]);
			switch(esTag){
				case 1:		// OD Update
					printed = 1;
					saved_printatom = vg.printatom;
					vg.printatom = vg.printsample;
					while (bb->bits_left > bits_to_leave) Validate_Object_Descriptor( bb );
					vg.printatom = saved_printatom;
					break;
				case 2:		// OD Remove
					printed = 1;
					while (bb->bits_left > bits_to_leave) { VALIDATE_FIELD  ("%d",  OD_ID, 10 ); };
					GetBits(bb,(bb->bits_left & 7),nil);	/* byte-align ourselves */
					break;
				case 3:		// ES Update
					printed = 1;
					VALIDATE_FIELD  ("%d",  OD_ID, 10 );
					GetBits(bb,(bb->bits_left & 7),nil);	/* byte-align ourselves */
					saved_printatom = vg.printatom;
					vg.printatom = vg.printsample;
					while (bb->bits_left > bits_to_leave) Validate_ES_Descriptor(bb, 0,0, false);
					vg.printatom = saved_printatom;
					break;
				case 4:		// ES Remove
					printed = 1;
					VALIDATE_FIELD  ("%d",  OD_ID, 10 );
					GetBits(bb,(bb->bits_left & 7),nil);	/* byte-align ourselves */
					while (bb->bits_left > bits_to_leave) VALIDATE_FIELD  ("%d",  ES_ID, 16 );
					break;
				case 5:		// IPMP update
					printed = 1;
					saved_printatom = vg.printatom;
					vg.printatom = vg.printsample;
					while (bb->bits_left > bits_to_leave) Validate_Random_Descriptor( bb, "Descriptor" );
					vg.printatom = saved_printatom;
					break;
				case 6:		// IPMP remove
					printed = 1;
					while (bb->bits_left > bits_to_leave) VALIDATE_FIELD  ("%d",  IPMP_DescrID, 8 );
					break;
				case 7:		// ES Remove Ref
					printed = 1;
					VALIDATE_FIELD  ("%d",  OD_ID, 10 );
					GetBits(bb,(bb->bits_left & 7),nil);	/* byte-align ourselves */
					while (bb->bits_left > bits_to_leave) VALIDATE_FIELD  ("%d",  Track_ref_Index, 16 );
					break;
				case 8:		// OD Execute
					printed = 1;
					while (bb->bits_left > bits_to_leave) { VALIDATE_FIELD  ("%d",  OD_ID, 10 ); };
					GetBits(bb,(bb->bits_left & 7),nil);	/* byte-align ourselves */
					break;
			}
		}
		else if (esTag<=0xc0) errprint("Validate_odsm_sample_Bitstream: OD Stream uses reserved command 0x%x\n",esTag);
		else if (esTag<=0xfe) warnprint("WARNING: Validate_odsm_sample_Bitstream: OD Stream uses user-private command 0x%x\n",esTag);
		else errprint("Validate_odsm_sample_Bitstream: OD Stream uses forbidden command 0x%x\n",esTag);
			
		if (printed == 0) {
			sampleprinthexdata((void*)bb->cptr, esSize);
			err = SkipBytes(bb, esSize); if (err) goto bail;
		}
		--vg.tabcnt; 
	}
	sampleprint("</OD_sample>\n" );
	
bail:
	if (err) {
		errprint("Validate_odsm_sample_Bitstream: %d\n",err);
	}
	return err;
}

//==========================================================================================

OSErr Validate_vide_sample_Bitstream( BitBuffer *bb, void *refcon )
{
	TrackInfoRec *tir = (TrackInfoRec *)refcon;
	OSErr err;
	UInt32* codec_specific;
	SampleDescriptionPtr sampleDescription;
	

// data from ES & state info is in tir->validatedSampleDescriptionRefCons
//    for global refcon use [0]
		
	codec_specific = &((tir->validatedSampleDescriptionRefCons)[tir->currentSampleDescriptionIndex - 1]);
	
	sampleDescription = tir->sampleDescriptions[tir->currentSampleDescriptionIndex];
	if (sampleDescription->head.sdType == 'avc1') {
	   while (bb->bits_left>0) {
			UInt32 nsize, size_field;
			size_field = codec_specific[0];
			nsize = GetBits(bb, size_field * 8, &err); if (err) goto bail;	
			Validate_NAL_Unit(bb,0,nsize);
			err = SkipBytes(bb, nsize); if (err) goto bail;
	   }
	} else
	{
		sampleprinthexdata((void*)bb->ptr, bb->length);
		// assumes the entire bit buffer is the sample
		err = SkipBytes(bb, bb->length); if (err) goto bail;
	}
	
	if (NumBytesLeft(bb) > 1) {
		err = tooMuchDataErr;
	}
	
bail:
	if (err) {
		errprint("Validate_vide_ES_Bitstream: %d\n",err);
	}
	return err;
}

//==========================================================================================

OSErr Validate_soun_sample_Bitstream( BitBuffer *bb, void *refcon )
{
	TrackInfoRec *tir = (TrackInfoRec *)refcon;
	OSErr err;
	UInt32* codec_specific;
	

// data from ES & state info is in tir->validatedSampleDescriptionRefCons
//    for global refcon use [0]
		
	sampleprinthexdata((void*)bb->ptr, bb->length);
	// assumes the entire bb is the sample
	codec_specific = &((tir->validatedSampleDescriptionRefCons)[tir->currentSampleDescriptionIndex - 1]);
	
	err = SkipBytes(bb, bb->length); if (err) goto bail;

	if (NumBytesLeft(bb) > 1) {
		err = tooMuchDataErr;
	}
	
bail:
	if (err) {
		errprint("Validate_soun_ES_Bitstream: %d\n",err);
	}
	return err;
}

//==========================================================================================


// validate values from VideoSpecificInfo in relation to videoProfileLevelIndication

OSErr CheckValuesInContext( UInt32 bufferSize, UInt32 maxBitrate, UInt32 avgBitrate, void *p_sc )
{
  PartialVideoSC *p_vsc = (void*)p_sc;
  OSErr err = noErr;
  char profString[100];
  int limitBitrate = 0;
  int limitBufferSize = 0;
  float fps_max;
  
  UInt32 widthMB, heightMB;		// in macroblocks
  
  widthMB = (p_vsc->volWidth+15)/16;
  heightMB = (p_vsc->volHeight+15)/16;
  
  switch(p_vsc->profileLevelInd){
  case 8:
    sprintf(profString,"SP0");
    p_vsc->maxMBsec = 1485;
    limitBitrate = 65536;
    limitBufferSize = 163840;
    break;
  case 1:
    sprintf(profString,"SP1");
    p_vsc->maxMBsec = 1485;
    limitBitrate = 65536;
    limitBufferSize = 163840;
    break;
  case 2:
    sprintf(profString,"SP2");
    p_vsc->maxMBsec = 5940;
    limitBitrate = 131072;
    limitBufferSize = 655360;
    break;
  case 3:
    sprintf(profString,"SP3");
    p_vsc->maxMBsec = 11880;
    limitBitrate = 393216;
    limitBufferSize = 655360;
    break;
  case 240:
    sprintf(profString,"ASP0");
    p_vsc->maxMBsec = 2970;
    limitBitrate = 131072;
    limitBufferSize = 163840;
    break;
  case 241:
    sprintf(profString,"ASP1");
    p_vsc->maxMBsec = 2970;
    limitBitrate = 131072;
    limitBufferSize = 163840;
    break;
  case 242:
    sprintf(profString,"ASP2");
    p_vsc->maxMBsec = 5940;
    limitBitrate = 393216;
    limitBufferSize = 655360;
    break;
  case 243:
    sprintf(profString,"ASP3");
    p_vsc->maxMBsec = 11880;
    limitBitrate = 786432;
    limitBufferSize =  655360;
    break;
  case 247:
    sprintf(profString,"ASP3b");
    p_vsc->maxMBsec = 11880;
    limitBitrate = 1536000;
    limitBufferSize = 1064960;
    break;

  default:
    sprintf(profString,"WARNING: unknown visual profile= %lu\n",p_vsc->profileLevelInd);
    if( p_vsc->profileLevelInd == 255 ){
      err = 1;
      errprint("invalid visual profile= %lu\n",p_vsc->profileLevelInd);
    }
    else
      warnprint("%s",profString);
  }

  fps_max = p_vsc->maxMBsec/ (widthMB * heightMB);

  if( maxBitrate > limitBitrate || bufferSize > limitBufferSize){
    err = 2;
    errprint("CheckValuesInContext: video profile limitations exceeded .. profile is %s  max bitrate is= %lu (limit is %lu)  buffer size is= %lu (limit is %lu) \n",
             profString, maxBitrate, limitBitrate, bufferSize, limitBufferSize);
  }
  else{
    warnprint("NOTE: using volHeight= %lu  volWidth= %lu  VideoProfileLevelID= %s -> the max. allowed average framerate is %.2f fps\n",
              p_vsc->volHeight, p_vsc->volWidth, profString, fps_max );
  }

  return err;
}

//==========================================================================================
