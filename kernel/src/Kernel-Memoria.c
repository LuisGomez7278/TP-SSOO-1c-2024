#include "../include/Kernel-Memoria.h"


    void atender_conexion_MEMORIA_KERNEL(){

//RECIBIR MENSAJE DE MEMORIA
    op_code codop = recibir_operacion(socket_memoria_kernel);
    if (codop == MENSAJE) {printf("LLego un mensaje\n");}
    else {printf("LLego otra cosa");}
    recibir_mensaje(socket_memoria_kernel, logger);

//ENVIAR MENSAJE A MEMORIA
    enviar_mensaje("Kernel manda mensaje a memoria", socket_memoria_kernel);
    log_info(logger, "Se envio el primer mensaje a memoria");
        
        bool continuarIterando=true;
       

        while (continuarIterando) {
            uint32_t cod_op = recibir_operacion(socket_memoria_kernel);   ////se queda esperando en recv por ser bloqueante
            switch (cod_op) {
            case MENSAJE:
                recibir_mensaje(socket_memoria_kernel,logger_debug);
                break;
            case CARGA_EXITOSA_PROCESO:                                     /// ABRO UN HILO POR CADA PROCESO QUE SE ENCOLA EN NEW ASI PUEDO SEGUIR ESCUCHANDO PROCESOS
                pthread_t hilo_de_Planificador_largo_plazo;                
                pthread_create(&hilo_de_Planificador_largo_plazo, NULL,(void*) carga_exitosa_en_memoria,NULL); 
                pthread_detach(hilo_de_Planificador_largo_plazo);
                break;
            case ERROR_AL_CARGAR_EL_PROCESO:
                    uint32_t sizeTotal;
                    uint32_t desplazamiento=0;
                    void* buffer= recibir_buffer(&sizeTotal,socket_memoria_kernel);
                    uint32_t PID = leer_de_buffer_uint32(buffer,&desplazamiento); 
                    t_pcb *pcb= buscar_pcb_por_PID_en_lista(lista_new,PID);
                    if(list_remove_element(lista_new,pcb)){
                        log_error(logger_debug,"Error al cargar el proceso PID: %u en memoria. Eliminado de NEW",PID);
                    }else{
                        log_error(logger_debug,"Error al cargar el proceso PID: %u en memoria. No se pudo eliminar de NEW",PID);
                    }
                break;
            case -1:
                log_error(logger_debug, "el MODULO DE MEMORIA SE DESCONECTO.");
                continuarIterando=false;
                break;
            default:
                log_warning(logger_debug,"KERNEL recibio una operacion desconocida.");
                break;
            }
        }


    
     
    
    }

void solicitud_de_creacion_proceso_a_memoria(uint32_t PID, char *leido){


    char** leido_array = string_split(leido, " ");  //SEPARO EL STRING LEIDO EN UN VECTOR DE STRING: 
    
//LE DOY VALOR A LAS VARIABLES A ENVIAR    

    //estructura: codigo operacion, pid, path_para_memoria
    op_code codigo_de_operacion=CREAR_PROCESO;
    char* path_para_memoria=leido_array[1];
    uint32_t tamanio=strlen(path_para_memoria);                      //--------------LO GUARDO EN UN uint32_t DE 32 BITS PORQUE EL STRLEN DEVUELVE 64 BITS 

//PREAPARO EL STREAM DE DATOS, LOS SERIALIZO Y ENVIO

    t_paquete *paquete= crear_paquete (codigo_de_operacion);
    agregar_a_paquete_uint32(paquete,PID);
    agregar_a_paquete_string(paquete,tamanio,path_para_memoria);
    enviar_paquete(paquete,socket_memoria_kernel);              //--------------ESTA FUNCION SERIALIZA EL PAQUETE ANTES DE ENVIARLO --quedaria un void*= cod_op||SIZE TOTAL||size path|| path
    eliminar_paquete(paquete);


}

void carga_exitosa_en_memoria(){  


//RECIBO EL PROCESO QUE CARGO EN MEMORIA

    uint32_t *sizeTotal=malloc(sizeof(uint32_t));
    uint32_t *desplazamiento=malloc(sizeof(int));
    *desplazamiento=0;
    void* buffer= recibir_buffer(sizeTotal,socket_memoria_kernel);
    uint32_t PID = 0; 

    if (buffer != NULL) {
    PID = leer_de_buffer_uint32(buffer, desplazamiento);
 
        
        log_info(logger_debug,"CARGA EXITOSA DEL PROCESO: PID= %u ",PID);
        log_info(logger, "Se crea el proceso con PID: %u en NEW",PID);

        free(sizeTotal);
        free(desplazamiento);
        free(buffer);
    
    } else {
        // Manejo de error en caso de que recibir_buffer devuelva NULL
        log_error(logger_debug,"Error al recibir el buffer");
    }


                                                    ///         GESTIONO LAS LISTAS DE ESTADO    

    sem_wait(&control_multiprogramacion);           ///         SOLO AVANZO SI LA MULTIPROGRAMACION LO PERMITE    --------------------------------------------------------------
    
    if (detener_planificacion)                      /// Si la PLANIFICACION ESTA DETENIDA QUEDO BLOQEUADO EN WAIT
    {
        sem_wait(&semaforo_plp);
    }
    
    t_pcb* pcb_ready= buscar_pcb_por_PID_en_lista(lista_new,PID);

    if(pcb_ready==NULL){
        log_error(logger_debug,"Error al buscar el proceso con PID= %u en la lista New",PID);
    }else{
        t_pcb pcb_copia=*pcb_ready;
        pthread_mutex_lock(&semaforo_new);        
        if (list_remove_element(lista_new, pcb_ready)){
        
            ingresar_en_lista(&pcb_copia, lista_ready, &semaforo_ready, &cantidad_procesos_en_algun_ready , READY); //loggeo el cambio de estado, loggeo el proceso si es cola ready/prioritario 
        }else{
        
            log_error(logger_debug,"Error al eliminar el elemento PID= u% de la lista NEW",PID);
        }
        pthread_mutex_unlock(&semaforo_new);
    }
}


t_pcb* buscar_pcb_por_PID_en_lista(t_list* lista, uint32_t pid_buscado){
	t_link_element* aux=lista->head;
    t_pcb* pcb_auxiliar=malloc(sizeof(t_pcb));
 

    while (aux!=NULL)
    {
        pcb_auxiliar= (t_pcb*) aux->data; 

    if (pcb_auxiliar->PID==pid_buscado){
        return pcb_auxiliar;
    }
    aux=aux->next;
    }
    
	return NULL;
}


