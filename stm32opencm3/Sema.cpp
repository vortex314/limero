#include <Log.h>
#include <Sema.h>
#include <FreeRTOS.h>
#include <semphr.h>

class FreeRtosSemaphore : public Sema {
		SemaphoreHandle_t xSemaphore = NULL;
		uint32_t counter=0;
	public:
		FreeRtosSemaphore() {
			xSemaphore = xSemaphoreCreateBinary();
			xSemaphoreGive(xSemaphore);
		}
		~FreeRtosSemaphore() {}

		void wait() {
			if (xSemaphoreTake(xSemaphore, (TickType_t)100000) == pdFALSE) {
				WARN(" xSemaphoreTake()  timed out ");
			}
			counter++;
			if ( counter != 1 ) {
				WARN(" =======> wait sema counter %d \n",counter);
			}
		}

		void release() {
			counter--;
			if ( counter != 0 ) {
				WARN(" ======> give sema counter %d \n",counter);
			}
			if (xSemaphoreGive(xSemaphore) == pdFALSE) {
				WARN("xSemaphoreGive() failed");
			}


		}
		bool taken() {
			return uxSemaphoreGetCount(xSemaphore)==0;
		}
   static Sema& create();
};

Sema& Sema::create() {return *new FreeRtosSemaphore();}
