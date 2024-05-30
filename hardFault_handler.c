/**
 * A hardfault handler for cortex M4
 * In addition to the core registers
 * the handler saves the entire stack of the violating context to a persistent memory
 */


/********************* HardFault Handler *******************************/

/**
 * Saving the data to a location on the RAM that isn't included in the linker file and will not be erased by reset
 * Another option is to save the data to the internal flash
 */
#define ERROR_HANDELING_MEMORY_ADDRESS (PROG_RAM_END) // end of RAM allocated by the linker file
#define ERROR_HANDELING_MEMORY_SIZE (RAM_END - PROG_RAM_END)

void memory_erase(uint32_t address, uint32_t length)
{
	memset((void*)address, 0, length);
}

void memory_write(uint32_t address, const void* data, uint32_t length)
{
	memcpy((void*)address, data, length);
}

void memory_read(uint32_t address, void* data, uint32_t length)
{
	memcpy(data, (void*)address, length);
}


/**
 * The SCB registers in the order they are defined in core_cm4.h
 */
typedef struct __attribute__((__packed__)) SCB_registers_t {
	uint32_t CFSR;
	uint32_t HFSR;
	uint32_t DFSR;
	uint32_t MMFAR;
	uint32_t BFAR;
	uint32_t AFSR;
}SCB_registers_t;

typedef struct __attribute__((__packed__)) core_registers_t {
	uint32_t R0;
	uint32_t R1;
	uint32_t R2;
	uint32_t R3;
	uint32_t R12;
	uint32_t LR;
	uint32_t PC;
	uint32_t PSR;
}core_registers_t;

/**
 * the dump will be saved to the memory in the following format
 */
typedef struct __attribute__((__packed__)) core_dump_t {
	SCB_registers_t SCB_registers;
	core_registers_t core_registers;
	uint8_t  context_stack[];
}core_dump_t;

// --------------------------------------------------------------------------------------
static inline uint32_t getMainStackBase(void)
{
	extern uint32_t _estack[]; //stack start address definition in the linker file
	return (uint32_t)_estack;
}

static inline uint32_t getTaskStackBase(uint32_t sp)
{
	/* return the start of the stack of the last running task
	 * can either use implementation for your own OS or even use a constant size like: sp + 1024*/
	return sp +1024;
}

static inline uint32_t getStackBase(uint32_t sp)
{
	/* The naked function does not pass the LR of the exception, instead of modifying it we'll uses another way to find the context.
	 * The hardfault is running on the main context, so the PSP hasn't changed, it's current value is the same value as it was at the time of the violation.
	 * This means that if the sp equals to the PSP, we are in a process context */
	if (sp == __get_PSP())
		return getTaskStackBase(sp);
	else
		return getMainStackBase();
}

// --------------------------------------------------------------------------------------

/**
 * read the last saved hardfault value if exist
 * buffer - the data will be returned in a format of core_dump_t
 * return - true: read successfull, false: no data exist
 */
bool hardFault_readSavedData(void* buffer, uint32_t bufferSize)
{
	memory_read(ERROR_HANDELING_MEMORY_ADDRESS, buffer, bufferSize);

	core_dump_t* core_dump_ptr = buffer;
	return core_dump_ptr->core_registers.PC != 0xFFFFFFFF;
}


/**
 * erase the saved hardfault data
 */
void hardFault_eraseSavedData(void)
{
	memory_erase(ERROR_HANDELING_MEMORY_ADDRESS, ERROR_HANDELING_MEMORY_SIZE);
}

// --------------------------------------------------------------------------------------

/**
 * called by the HardFault_Handler
 * stores the core dump and stack to the memory in the format of core_dump_t and reboot the system
 */
static void prvGetRegistersFromStack(uint32_t *pulFaultStackAddress)
{
	core_registers_t* core_registers = (core_registers_t*)pulFaultStackAddress;
	uint32_t memoryWriteAddress = ERROR_HANDELING_MEMORY_ADDRESS;
	hardFault_eraseSavedData();

	/* save the SCB registers */
	SCB_registers_t* SCB_registers = (SCB_registers_t*)&(SCB->CFSR);
	memory_write(memoryWriteAddress, (void*)SCB_registers, sizeof(SCB_registers_t));
	memoryWriteAddress += sizeof(SCB_registers_t);

	/* save the core registers and the stack of the crash */
	uint32_t stackBase = getStackBase((uint32_t)pulFaultStackAddress);
	uint32_t stackSize = stackBase - (uint32_t)pulFaultStackAddress;
	uint32_t sizeLeftForStackDump = ERROR_HANDELING_MEMORY_ADDRESS + ERROR_HANDELING_MEMORY_SIZE - memoryWriteAddress;
	uint32_t NumOfbyteToWrite = MIN(stackSize, sizeLeftForStackDump);
	memory_write(memoryWriteAddress, (void*)pulFaultStackAddress, NumOfbyteToWrite);
	
#ifdef DEBUG
	__ASM volatile("BKPT #01"); //force a breakpoint
	for (;;) ;
#endif
	NVIC_SystemReset();
}

/**
 * Hard Fault Handling Code (Taken from FreeRTOS)
 * The fault handler implementation calls a function prvGetRegistersFromStack(uint32_t *pulFaultStackAddress). 
 * pulFaultStackAddress will contain values of 8 core registers: r0, r1, r2, r3, r12, lr, pc, psr
 */
__attribute__((naked)) void HardFault_Handler(void)
{
	__asm volatile
	(
	    " tst lr, #4                                                \n" //check the LR to see if we used the MSP or PSP. see cortexM4 docuemntation
	    " ite eq                                                    \n"
	    " mrseq r0, msp                                             \n" //if we used the MSP copy it to r0
	    " mrsne r0, psp                                             \n" //if we used the PSP copy it to r0
	    " ldr r1, [r0, #24]                                         \n"
	    " ldr r2, handler2_address_const                            \n"
	    " bx r2                                                     \n" //jump to prvGetRegistersFromStack(uint32_t *pulFaultStackAddress)
	    " handler2_address_const: .word prvGetRegistersFromStack    \n"
	);
}
