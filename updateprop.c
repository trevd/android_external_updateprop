/*
 * Copyright (C) 2015 Trevor Drake
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>
#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>

#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>
#ifndef PA_SIZE
#define PA_SIZE         (128 * 1024)
#endif
extern void *__system_property_area__;

int main(int argc, char** argv)
{

    /* Check the command line arguements */
    if ( argc < 3 ){
	    fprintf(stderr,"\nusage updateprop <name> <value>\n");
	    fprintf(stderr,"Override readonly property values. requires root\n\n");
	    return -1;
    }
    /* Check we are running as root */
    if ( getuid() != 0 ){
	fprintf(stderr,"\nMust be running as root\n");
	return -1 ;
    }

    /* Check the property name is valid */
    char* property_name = argv[1];
    if ( property_name == NULL ) {
	fprintf(stderr,"\nInvalid Property Name\n");
	return -1;
    }
    /* Check the property name length is valid */
    size_t property_name_length = strnlen(property_name,PROP_NAME_MAX);
    if( property_name_length >= PROP_NAME_MAX ){
	fprintf(stderr,"\nProperty Name too long\n");
	return -1 ;
    }
    /* Check the property value is valid */
    char* property_value = argv[2];
    if ( property_value == NULL ) {
	fprintf(stderr,"\nInvalid Property Value\n");
	return -1;
    }
    /* Check the property value length is valid */
    size_t property_value_length = strnlen(property_value,PROP_VALUE_MAX);
    if( property_value_length >= PROP_VALUE_MAX ){
	fprintf(stderr,"\nProperty Value too long\n");
	return -1 ;
    }

    /* __system_property_area__ contains the address of mmapped file /dev/__properties__
       which is mapped into the process address space as part of the program setup
       procedure in crt0. The file is readonly and has been mapped as readonly. This
       means we cannot use mprotect to change the access permission and instead we
       need to first unmap the file and reopen it as r/w
    */
    if ( munmap(__system_property_area__,PA_SIZE)  == -1 ){
	fprintf(stderr,"munmap failed  __system_property_area__=%p %d %s\n",__system_property_area__,errno,strerror(errno));
	return -1 ;
    }
    /* Reopen the property file r/w */
    int fd = open(PROP_FILENAME, O_RDWR | O_NOFOLLOW | O_CLOEXEC);
    if ( fd < 0 ){
	fprintf(stderr,"open property file failed %d %s\n",errno,strerror(errno));
	return -1 ;
    }

    /* Map the property file fd as r/w and set that as the __system_property_area__   */
    __system_property_area__ = mmap(NULL, PA_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(__system_property_area__ == MAP_FAILED){
        fprintf(stderr,"MAP_FAILED __system_property_area__=%p %d %s\n",__system_property_area__,errno,strerror(errno));
        close(fd);
        return -1 ;
    }

    /* Get the prop_info for the property */
    prop_info *pi = (prop_info*) __system_property_find(property_name);
    if(pi == NULL) {
	fprintf(stderr,"Property Not Found %s\n",property_name);
        close(fd);
        return -1 ;
    }
    /* Set the property to the new value */
    if ( __system_property_update(pi, property_value, property_value_length) == -1 ){
	fprintf(stderr,"Failed to set Property: %s=%s\n",property_name,property_value);
	close(fd);
	return -1;
    }
    /* Check the Property Value */
    char property_value_return[PROP_VALUE_MAX];
    if ( __system_property_get(property_name,property_value_return) == -1 ){
	fprintf(stderr,"Could Not Check Property %s\n",property_name);
	close(fd);
	return -1 ;
    }
    if( strncmp(property_value, property_value_return,property_value_length) ){
	fprintf(stderr,"Property %s has not been updated\n",property_name);
	close(fd);
	return -1 ;

    }

    fprintf(stdout,"%s=%s\n",property_name,property_value_return);
    close(fd);
    return 0;

}
