/*
 * Copyright (c) 2001, 2002, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Driver code for whale mating problem
 */
#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

/*
 * 08 Feb 2012 : GWA : Driver code is in kern/synchprobs/driver.c. We will
 * replace that file. This file is yours to modify as you see fit.
 *
 * You should implement your solution to the whalemating problem below.
 */

// 13 Feb 2012 : GWA : Adding at the suggestion of Isaac Elbaz. These
// functions will allow you to do local initialization. They are called at
// the top of the corresponding driver code.

static void getquadrants(unsigned long, int *);

struct cv *cvWaitingMales;
struct cv *cvWaitingFemales;
struct semaphore *semMale;
struct semaphore *semFemale;
struct lock *lkMale;
struct lock *lkFemale;


void whalemating_init() {
	cvWaitingMales = cv_create("waitingmales");
	cvWaitingFemales = cv_create("waitingfemales");
	semMale = sem_create("semMale",0);
	semFemale = sem_create("semFemale",0);
	lkMale = lock_create("lkMale");
	lkFemale = lock_create("lkFemale");
	return;
}

// 20 Feb 2012 : GWA : Adding at the suggestion of Nikhil Londhe. We don't
// care if your problems leak memory, but if you do, use this to clean up.

void whalemating_cleanup() {
	sem_destroy(semMale);
	sem_destroy(semFemale);
	cv_destroy(cvWaitingMales);
	cv_destroy(cvWaitingFemales);
	lock_destroy(lkMale);
	lock_destroy(lkFemale);
	return;
}

void
male(void *p, unsigned long which)
{
	struct semaphore * whalematingMenuSemaphore = (struct semaphore *)p;
	(void)which;

	male_start();
	V(semMale);
	lock_acquire(lkMale);
	cv_wait(cvWaitingMales, lkMale);
	lock_release(lkMale);
	male_end();

	// 08 Feb 2012 : GWA : Please do not change this code. This is so that your
	// whalemating driver can return to the menu cleanly.
	V(whalematingMenuSemaphore);
	return;
}

void
female(void *p, unsigned long which)
{
	struct semaphore * whalematingMenuSemaphore = (struct semaphore *)p;
	(void)which;

	female_start();
	V(semFemale);
	lock_acquire(lkFemale);
	cv_wait(cvWaitingFemales, lkFemale);
	lock_release(lkFemale);

	female_end();

	// 08 Feb 2012 : GWA : Please do not change this code. This is so that your
	// whalemating driver can return to the menu cleanly.
	V(whalematingMenuSemaphore);
	return;
}

void
matchmaker(void *p, unsigned long which)
{
	struct semaphore * whalematingMenuSemaphore = (struct semaphore *)p;
	(void)which;

	matchmaker_start();
	P(semMale);
	P(semFemale);
	lock_acquire(lkMale);
	lock_acquire(lkFemale);
	cv_signal(cvWaitingMales,lkMale);

	cv_signal(cvWaitingFemales,lkFemale);
	matchmaker_end();

	// 08 Feb 2012 : GWA : Please do not change this code. This is so that your
	// whalemating driver can return to the menu cleanly.
	V(whalematingMenuSemaphore);
	return;
}

/*
 * You should implement your solution to the stoplight problem below. The
 * quadrant and direction mappings for reference: (although the problem is,
 * of course, stable under rotation)
 *
 *   | 0 |
 * --     --
 *    0 1
 * 3       1
 *    3 2
 * --     --
 *   | 2 | 
 *
 * As way to think about it, assuming cars drive on the right: a car entering
 * the intersection from direction X will enter intersection quadrant X
 * first.
 *
 * You will probably want to write some helper functions to assist
 * with the mappings. Modular arithmetic can help, e.g. a car passing
 * straight through the intersection entering from direction X will leave to
 * direction (X + 2) % 4 and pass through quadrants X and (X + 3) % 4.
 * Boo-yah.
 *
 * Your solutions below should call the inQuadrant() and leaveIntersection()
 * functions in drivers.c.
 */

// 13 Feb 2012 : GWA : Adding at the suggestion of Isaac Elbaz. These
// functions will allow you to do local initialization. They are called at
// the top of the corresponding driver code.

struct lock *lkPlanning;
struct lock *lkQuadrant[4];

void stoplight_init() {
	lkQuadrant[0] = lock_create("lkZero");
	lkQuadrant[1] = lock_create("lkOne");
	lkQuadrant[2] = lock_create("lkTwo");
	lkQuadrant[3] = lock_create("lkThree");
	lkPlanning = lock_create("lkPlanning");
	return;
}

// 20 Feb 2012 : GWA : Adding at the suggestion of Nikhil Londhe. We don't
// care if your problems leak memory, but if you do, use this to clean up.

void stoplight_cleanup() {
	lock_destroy(lkQuadrant[0]);
	lock_destroy(lkQuadrant[1]);
	lock_destroy(lkQuadrant[2]);
	lock_destroy(lkQuadrant[3]);
	lock_destroy(lkPlanning);
	return;
}

void getquadrants(unsigned long direction, int* ret)
{
	ret[0] = direction;
	ret[1] = (direction + 3) % 4;
	ret[2] = (direction  + 2) % 4;

}

void
gostraight(void *p, unsigned long direction)
{
	struct semaphore * stoplightMenuSemaphore = (struct semaphore *)p;

	int quad[3];
	getquadrants(direction, quad);

	lock_acquire(lkPlanning);

	lock_acquire(lkQuadrant[quad[0]]);
	lock_acquire(lkQuadrant[quad[1]]);

	lock_release(lkPlanning);
	inQuadrant(quad[0]);
	inQuadrant(quad[1]);
	lock_release(lkQuadrant[quad[0]]);
	leaveIntersection();
	lock_release(lkQuadrant[quad[1]]);
	// 08 Feb 2012 : GWA : Please do not change this code. This is so that your
	// stoplight driver can return to the menu cleanly.
	V(stoplightMenuSemaphore);
	return;
}

void
turnleft(void *p, unsigned long direction)
{
	struct semaphore * stoplightMenuSemaphore = (struct semaphore *)p;
	int quad[3];
	getquadrants(direction, quad);

	lock_acquire(lkPlanning);

	lock_acquire(lkQuadrant[quad[0]]);
	lock_acquire(lkQuadrant[quad[1]]);
	lock_acquire(lkQuadrant[quad[2]]);

	lock_release(lkPlanning);
	inQuadrant(quad[0]);
	inQuadrant(quad[1]);
	lock_release(lkQuadrant[quad[0]]);
	inQuadrant(quad[2]);
	lock_release(lkQuadrant[quad[1]]);
	leaveIntersection();
	lock_release(lkQuadrant[quad[2]]);

	// 08 Feb 2012 : GWA : Please do not change this code. This is so that your
	// stoplight driver can return to the menu cleanly.
	V(stoplightMenuSemaphore);
	return;
}

void
turnright(void *p, unsigned long direction)
{
	struct semaphore * stoplightMenuSemaphore = (struct semaphore *)p;

	lock_acquire(lkPlanning);

	lock_acquire(lkQuadrant[direction]);
	inQuadrant(direction);
	leaveIntersection();
	lock_release(lkQuadrant[direction]);

	lock_release(lkPlanning);
	// 08 Feb 2012 : GWA : Please do not change this code. This is so that your
	// stoplight driver can return to the menu cleanly.
	V(stoplightMenuSemaphore);
	return;
}
