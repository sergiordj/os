mascara: .word 0 //Declara una variable del tamaño palabra y la inicializa en 0
	.syntax unified
	.global PendSV_Handler

	/* declaro acceso a la rutina de seleccion de proximo contexto */
	.extern getNextContext

	.text
	.thumb_func
PendSV_Handler:

	/* verificamos si el contexto anterior utilizó la fpu y tomamos las medidas*/
	tst lr, 0x10 //comparamos con la máscara el bit 4
	it eq //si es 0, pusheamos los registros de la fpu
	vpusheq {s16-s31}

	push {r4-r11, lr}

	/*Por convención (AAPCS), paso el parámetro en R0 y devuelvo
	el valor en R0.
	*/
	mrs r0, msp /* r0 = msp */
	bl getNextContext /* r0 = siguiente puntero de pila
	uso la instrucción bl (branch with link) */

	msr msp, r0 /* msp = r0 */

	pop {r4-r11,lr}

	/* verificamos si el contexto que entra utilizó la fpu y tomamos las medidas*/
	tst lr, 0x10 //comparamos con la máscara el bit 4
	it eq //si es 0, popeamos los registros de la fpu
	vpopeq {s16-s31}

	bx lr /* retorno de interrupción,
debería recuperar el contexto armado en task_create */
