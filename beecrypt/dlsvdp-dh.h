/*
 * Copyright (c) 2000, 2002 Virtual Unlimited B.V.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*!\file dlsvdp-dh.h
 * \brief Diffie-Hellman algorithm, headers.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup DL_m DL_dh_m 
 */

#ifndef _DLSVDP_DH_H
#define _DLSVDP_DH_H

#include "beecrypt/dldp.h"
#include "beecrypt/dlkp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef dldp_p dhparam;
typedef dlkp_p dhkp;

BEECRYPTAPI
int dlsvdp_pDHSecret(const dhparam*, const mpnumber*, const mpnumber*, mpnumber*);

#ifdef __cplusplus
}
#endif

#endif
