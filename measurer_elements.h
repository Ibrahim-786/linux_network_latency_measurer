/*
 * network latency measurer
 * Copyright (C) 2018  Ricardo Biehl Pasquali <pasqualirb@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * 14/05/2018
 */

#ifndef MEASURER_ELEMENTS_H
#define MEASURER_ELEMENTS_H

#include "sender.h" /* struct sender */
#include "storer.h" /* struct storer */
#include "receiver.h" /* struct receiver */

/* TODO: make it not pointers and place it in measurer structure */
struct measurer_elements {
	struct sender   *sender;
	struct storer   *storer;
	struct receiver *receiver;
};

#endif /* MEASURER_ELEMENTS_H */

