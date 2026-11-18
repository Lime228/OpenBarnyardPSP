SECTIONS
{
	.sceStub.text :
	{
		KEEP( *(.sceStub.text) )
		KEEP( *(.sceStub.text.*) )
	}

	.lib.stub.top : { KEEP( *(.lib.stub.top) ) }
	.lib.stub :
	{
		KEEP( *(.lib.stub) )
		KEEP( *(.lib.stub.entry.*) )
	}
	.lib.stub.btm : { KEEP( *(.lib.stub.btm) ) }

	.lib.ent.top : { KEEP( *(.lib.ent.top) ) }
	.lib.ent : { KEEP( *(.lib.ent) ) }
	.lib.ent.btm : { KEEP( *(.lib.ent.btm) ) }

	.rodata.sceResident :
	{
		KEEP( *(.rodata.sceResident) )
		KEEP( *(.rodata.sceResident.*) )
	}
	.rodata.sceModuleInfo : { KEEP( *(.rodata.sceModuleInfo) ) }
	.rodata.sceNid :
	{
		KEEP( *(.rodata.sceNid) )
		KEEP( *(.rodata.sceNid.*) )
	}
	.rodata.sceVstub : { KEEP( *(.rodata.sceVstub) ) }
}
INSERT AFTER .text;
