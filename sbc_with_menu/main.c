#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <MQTTClient.h>
#include <unistd.h>
#include <stdbool.h>

#include "topicos.h"
#include "lista_encadeada.h"

#define IP "tcp://test.mosquitto.org:1883" /*mudar para "tcp://10.0.0.101:1883"*/

#define CLIENTID "MQTTCClientID"
#define USER "aluno"
#define PASSWORD "@luno*123"

#define QOS 1

/*COMANDOS DE REQUISIÇÃO*/
#define SITUACAO_ATUAL_NODE 0x03
#define SOLICITA_ENTRADA_ANALOGICA 0x04
#define SOLICITA_ENTRADA_DIGITAL 0x05
#define TOGGLE_LED 0x06
/*COMANDOS DE RESPOSTA*/
#define NODE_COM_PROBLEMA 0x1F
#define NODE_FUNCIONANDO 0x00
#define MEDIDA_ENTRADA_ANALOGICA 0x01
#define ESTADO_ENTRADA_DIGITAL 0x02

/*Variaveis para configurar mqtt*/
MQTTClient client;
int rc;

/*Listas encadeadas para armazenar historico*/
Lista *list_historic_A0;
Lista *list_historic_D0;
Lista *list_historic_D1;

void publish(MQTTClient client, char *topic, char *payload);

volatile MQTTClient_deliveryToken deliveredtoken;

/*Confirmação da mensagem*/
void delivered(void *context, MQTTClient_deliveryToken dt)
{
    printf("Message with token value %d delivery confirmed\n", dt);
    deliveredtoken = dt;
}

/*Verifica se a lista possui mais de 10 medicoes no historico*/
bool list_full(Lista *list)
{
    if (list->tam >= 10)
    {
        return true;
    }
    return false;
}

/*Verifica se o historico possui mais de 10 medicoes e adiciona e remove adequadamente*/
void add_medicao_historico(Lista *list, char payloadptr)
{
    char caractere = payloadptr + '0';
    if (!list_full(list))
    {
        /*adiciona medicao mais nova no inicio do historico (primeira da lista)*/
        inserirInicio(list, caractere);
    }
    else
    {

        /*remove medicao mais antiga do historico (ultima da lista)*/
        remover(list, list->fim->valor);
        /*adiciona medicao mais nova no inicio do historico (primeira da lista)*/
        inserirInicio(list, caractere);
    }
}

/*int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    int i;
    char* payloadptr;
    printf("Message arrived\n");
    printf("     topic: %s\n", topicName);
    printf("   message: ");
    payloadptr = message->payload;
    for(i=0; i<message->payloadlen; i++)
    {
        putchar(*payloadptr++);
    }
    putchar('\n');
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}*/

/*Tratamento da notificação para o sbc pegar os dados dos tópicos que está inscrito, utilizada pela função MQTTClient_setCallbacks();*/
int callback_receive_data(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    char *payloadptr;
    printf("Message arrived\n");
    printf("     topic: %s\n", topicName);
    printf("   message: ");

    payloadptr = message->payload;

    puts(payloadptr);

    if (strcmp(topicName, SENSOR_A0_TOPIC) == 0)
    {
        add_medicao_historico(list_historic_A0, payloadptr[1]);
        /*printf("Medição Adicionada: %s", payloadptr[1]);*/
    }
    else if (strcmp(topicName, SENSOR_D0_TOPIC) == 0)
    {
        add_medicao_historico(list_historic_D0, payloadptr[1]);
        /*printf("Medição Adicionada: %s", payloadptr[1]);*/
    }
    else if (strcmp(topicName, SENSOR_D1_TOPIC) == 0)
        ;
    {
        add_medicao_historico(list_historic_D1, payloadptr[1]);
        /*printf("Medição Adicionada: %s", payloadptr[1]);*/
    }

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

/*Mensagem para o caso de conexão perdida*/
void connlost(void *context, char *cause)
{
    printf("\nConnection lost\n");
    printf("     cause: %s\n", cause);
}

/*Função que publica mensagens num tópico*/
void publish(MQTTClient client, char *topic, char *payload)
{
    MQTTClient_message pubmsg = MQTTClient_message_initializer;

    pubmsg.payload = payload;
    pubmsg.payloadlen = strlen(pubmsg.payload);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;
    MQTTClient_deliveryToken token;
    MQTTClient_publishMessage(client, topic, &pubmsg, &token);
    MQTTClient_waitForCompletion(client, token, 1000L);
}

/*------------------------------------------------ COMEÇA AQUI ------------------------------------------------ */

/*Faz subscribe nos tópicos necessários no início do programa*/
void start_subscribe_topics(MQTTClient client)
{
    MQTTClient_subscribe(client, SBC_CONFIG_TIME_TOPIC, QOS);
    MQTTClient_subscribe(client, SENSOR_A0_TOPIC, QOS);
    MQTTClient_subscribe(client, SENSOR_D0_TOPIC, QOS);
    MQTTClient_subscribe(client, SENSOR_D1_TOPIC, QOS);
}

/*Configuração de conexão do mqtt e criação do client*/
void mqtt_config()
{
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.username = USER;
    conn_opts.password = PASSWORD;

    MQTTClient_create(&client, IP, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_setCallbacks(client, NULL, connlost, callback_receive_data, delivered);

    rc = MQTTClient_connect(client, &conn_opts);

    if (rc != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to connect, return code %d\n", rc);
        exit(EXIT_FAILURE);
    }

    printf("MQTT Configurado\n");
}

void menu_comandos()
{
    int opcao;

    do
    {
        char dado[2];
        printf("1 - Situacao do node\n");
        printf("2 - Sensor A0\n");
        printf("3 - Sensor D0\n");
        printf("4 - Sensor D1\n");
        printf("5 - Led Toggle\n");
        printf("6 - Sair\n");

        scanf("%d", &opcao);
        system("clear");

        switch (opcao)
        {
        case 1:
            dado[0] = SITUACAO_ATUAL_NODE;
            dado[1] = 0;
            publish(client, COMMAND_TO_ESP_TOPIC, dado);
            break;
        case 2:
            dado[0] = SOLICITA_ENTRADA_ANALOGICA;
            dado[1] = 0;
            publish(client, COMMAND_TO_ESP_TOPIC, dado);
            break;
        case 3:
            dado[0] = SOLICITA_ENTRADA_DIGITAL;
            dado[1] = 0;
            publish(client, COMMAND_TO_ESP_TOPIC, dado);
            break;
        case 4:
            dado[0] = SOLICITA_ENTRADA_DIGITAL;
            dado[1] = 1;
            publish(client, COMMAND_TO_ESP_TOPIC, dado);
            break;
        case 5:
            dado[0] = TOGGLE_LED;
            dado[1] = 0; /* Qualquer dado para esse byte*/
            publish(client, COMMAND_TO_ESP_TOPIC, dado);
            break;
        case 6:
            break;
        }
    } while (opcao != 6);
}

void menu()
{
    int opcao;
    printf("---------Menu---------\n");
    printf("1 - Menu Comandos\n");
    scanf("%d", &opcao);
    system("clear");

    switch (opcao)
    {
    case 1:
        menu_comandos();
        break;
    default:
        break;
    }
}

int main(int argc, char *argv[])
{
    mqtt_config();
    /*start_subscribe_topics(client);*/
    while (true)
    {
        menu();
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    return rc;
}