#include <stdio.h> // Pour printf()
#include <stdlib.h> // Pour exit()
#include <string.h> // Pour memset()
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h> // pour memset
#include <netinet/in.h> // pour struct sockaddr_in
#include <arpa/inet.h> // pour htons et inet_aton
#include <unistd.h> // pour sleep
#include <poll.h> // pour poll()

#define PORT IPPORT_USERRESERVED // = 5000
#define LG_MESSAGE 256
#define LG_LOGIN 50
#define MAX_USERS 10
int temp; // va copier j (client  qui envoi le message)

void Client();

typedef struct User
{
	int socketclient;
	char login[LG_LOGIN];
}user;
	
int socketEcoute;
struct sockaddr_in pointDeRencontreLocal;
socklen_t longueurAdresse;
int socketDialogue;
struct sockaddr_in pointDeRencontreDistant;
char messageEnvoi[LG_MESSAGE]; 
char messageRecu[LG_MESSAGE]; 
user users[MAX_USERS];
struct pollfd pollfds[MAX_USERS + 1];
int ecrits, lus; // nb d’octets ecrits et lus
int retour;

 
int main(int argc, char const *argv[])
{


	memset(users, '\0', MAX_USERS*sizeof(user));

	// Crée un socket de communication
	socketEcoute = socket(PF_INET, SOCK_STREAM, 0);
	// 0 indique que l’on utilisera le protocole par défaut associé à SOCK_STREAM soit TCP
	
	// Teste la valeur renvoyée par l’appel système
	socket(PF_INET, SOCK_STREAM, 0);

	if(socketEcoute < 0){
		
		perror("socket"); // Affiche le message d’erreur
		exit(-1); // On sort en indiquant un code erreur
	}

	printf("Socket créée avec succès ! (%d)\n", socketEcoute);

	// On prépare l’adresse d’attachement locale
	longueurAdresse = sizeof(struct sockaddr_in);
	memset(&pointDeRencontreLocal, 0x00, longueurAdresse);
	pointDeRencontreLocal.sin_family = PF_INET;
	pointDeRencontreLocal.sin_addr.s_addr = htonl(INADDR_ANY);
	// toutes les interfaces locales disponibles
	pointDeRencontreLocal.sin_port = htons(PORT);

	// On demande l’attachement local de la socket
	if((bind(socketEcoute, (struct sockaddr *)&pointDeRencontreLocal, longueurAdresse)) < 0){
		
		perror("bind");
		exit(-2);
	}

	printf("Socket attachée avec succès !\n");

	// On fixe la taille de la file d’attente à 5 (pour les demandes de connexion non encore traitées)
	if(listen(socketEcoute, 5) < 0){
		
		perror("listen");
		exit(-3);
	}
	
	printf("Socket placée en écoute passive ...\n");
	// boucle d’attente de connexion : en théorie, un serveur attend indéfiniment !
	
	while(1){
		int nevents, i, j;
		int nfds = 0;

		// Liste des sockets à écouter
		pollfds[nfds].fd = socketEcoute;
		pollfds[nfds].events = POLLIN;
		pollfds[nfds].revents = 0;

		nfds++;

		// Remplissage du tableau avec les infos (pollfs) des utilisateurs connectés
		for (i = 0; i < MAX_USERS; i++){
			
			if (users[i].socketclient != 0){
				
				pollfds[nfds].fd = users[i].socketclient;
				pollfds[nfds].events = POLLIN;
				pollfds[nfds].revents = 0;

				nfds++;
			}
		}

		// Demander à paul s'il a vu des évènements / Paul au lieu de Poll c'est drôle....
		nevents = poll(pollfds, nfds, -1); 

		// Si poll a vu des évènements
		if (nevents > 0){
			
			for (i = 0; i < nfds; i++){
				
				if (pollfds[i].revents != 0){
					// S'il s'agit d'un évènement de la socket d'écoute (= nouvel utilisateur)
					
					if (pollfds[i].fd == socketEcoute){
						
						socketDialogue = accept(socketEcoute, (struct sockaddr *)&pointDeRencontreDistant, &longueurAdresse);
						if (socketDialogue < 0){
							perror("accept");
							exit(-4);
						}


						// Ajout de l'utilisateur
						for (j = 0; j < MAX_USERS; j++){
							// S'il y a une place de libre dans le tableau des utilisateurs connectés, on ajoute le nouvel utilisateur au tableau
							if (users[j].socketclient == 0){
								users[j].socketclient = socketDialogue;

								snprintf(users[j].login, LG_LOGIN, "anonymous%d", socketDialogue-4);//-4 pour commencer a 0 en position !!
								printf("Ajout de l'utilisateur %s en position %d\n", users[j].login, j);
								dprintf(users[j].socketclient, "[SERVEUR]  Version du programme : 1.3\n");
								dprintf(users[j].socketclient, "[SERVEUR]  Bienvenue sur mon projet !\n");
								dprintf(users[j].socketclient, "*-----------------------------------*\n");
								dprintf(users[j].socketclient, "<help> pour avoir les fonctions !\n");
								dprintf(users[j].socketclient, "*-----------------------------------*\n");
								for(int i=0;i<MAX_USERS;i++){
									if(strcmp(users[i].login,"\0") != 0){ //permet de ne pas flood le serveur
										dprintf(users[i].socketclient,"[SERVEUR] L'utilisateur %s en position %d a entrer le chat\n\n",users[j].login, j);
									}
								}
								printf("*-----------------------------------*\n");//le serveur est flood donc on soulage l'affichage en attendant une version supérieur
								break;
							}

							// Si aucune place n'a été trouvée
							if (j == MAX_USERS){
								printf("Plus de place de disponible pour ce nouvel utilisateur !\n");
								close(socketDialogue);
							}
						}
					}
					
					// Sinon, il s'agit d'un évènement d'un utilisateur (message, commande, etc)
					else{
						// On cherche quel utilisateur a fait la demande grâce à sa socket
						for (j = 0; j < MAX_USERS; j++){
							if (users[j].socketclient == pollfds[i].fd){
								temp=j;
								break;
							}
						}

						// Si aucun utilisateur n'a été trouvé
						if (j == MAX_USERS){
							printf("Utilisateur inconnu\n");
							break;
						}
						
						// On réceptionne les données du client
						lus = read(pollfds[i].fd, messageRecu, LG_MESSAGE*sizeof(char));

						switch (lus){
							case -1 : /* Une erreur */
								perror("read");
								exit(-5);

							case 0 : /* La socket est fermée */
								printf("L'utilisateur %s en position %d a quitté le chat\n", users[j].login, j);
								for(int i=0;i<MAX_USERS;i++){
									if(strcmp(users[i].login,"\0") != 0){
										dprintf(users[i].socketclient,"[SERVEUR] L'utilisateur %s en position %d a quitté le chat\n\n",users[j].login, j);
									}
								}
								memset(&users[j], '\0', sizeof(users[j].login));

							default: /* Réception de n octets */
								printf("Message reçu de %s : %s (%d octets)\n\n", users[j].login, messageRecu, lus);
								Client(messageRecu);
								memset(messageRecu, '\0', LG_MESSAGE*sizeof(char));
						}
					}
				}
			}
			
		}

		else{
			printf("poll() a renvoyé %d\n", nevents);
		}
	}

	// On ferme la ressource avant de quitter
	close(socketEcoute);
	
	return 0;
}

void Client(char *messageRecu)
{
	//léger soucis avec le non changement du login, on devrait stopper le programme et attendre de changer le login inutilisable, ici on doit refaire une boucle
	if (strncmp("<login>",messageRecu,6) == 0)
	{
		char templogin[50];
		char newtemp[50];
		
		dprintf(users[temp].socketclient, "[SERVEUR] Ancien login : %s\n",users[temp].login);
		dprintf(users[temp].socketclient, "*-----------------------------------*\n");	
		sscanf(messageRecu, "<login> %s",newtemp);
		int j=0;

		
		for(int i=0;i<MAX_USERS;i++){
			if(strcmp(newtemp,users[i].login)==0){
				dprintf(users[temp].socketclient,"[SERVEUR] Veuillez choisir un autre login, celui la étant déja prit \n");
				j=1;
			}
		}
			
			if(strcmp(newtemp,"srv")==0){
				dprintf(users[temp].socketclient,"[SERVEUR] Veuillez choisir un autre login, celui la étant réservé au serveur \n");
				j=1;
			}	
			else if(j==0){
				strncpy(templogin,users[temp].login,50);
				sscanf(messageRecu, "<login> %s", users[temp].login);

				dprintf(users[temp].socketclient, "[SERVEUR] Nouveau login : %s\n", users[temp].login);
				dprintf(users[temp].socketclient, "*-----------------------------------*\n");
				printf("[SERVEUR] %s s'est renommé : %s\n", templogin, users[temp].login);	
			}	
		for(int i=0;i<MAX_USERS;i++){
			if(strcmp(users[i].login,"\0") != 0){
				dprintf(users[i].socketclient,"[SERVEUR] %s s'est renommé : %s\n\n", templogin, users[temp].login);
			}
		}
	
	}
		
	
		
	
	else if (strncmp("<version>",messageRecu,9) == 0)
	{
		
		dprintf(users[temp].socketclient, "<version 1.1> \n");

	}

	else if (strncmp("<help>",messageRecu,6) == 0)
	{
		dprintf(users[temp].socketclient, "*-----------------------------------*\n");
		dprintf(users[temp].socketclient, "Liste des fonctions :\n");
		dprintf(users[temp].socketclient, "  -  <login>   :   modifier son login\n");
		dprintf(users[temp].socketclient, "  -  <list>    :   liste les logins\n");
		dprintf(users[temp].socketclient, "  -  <msg>    :   envoyer un message\n");
		dprintf(users[temp].socketclient, "  -  <exit>  :  déconnexion\n");
		dprintf(users[temp].socketclient, "*-----------------------------------*\n");
	}

	else if (strncmp("<list>",messageRecu,6) == 0)
	{
		dprintf(users[temp].socketclient,"[SERVEUR] ");
		for(int i = 0; i<MAX_USERS; i++){

			dprintf(users[temp].socketclient,"%s |",users[i].login);
		}
		dprintf(users[temp].socketclient,"\n");
	}

	else if (strncmp("<msg>",messageRecu,5) == 0)
	{
		char tempmsg[250];
		*tempmsg='\0';
		
		char templogin2[50];
		
		sscanf(messageRecu, "<msg> %s %s %[\001-\377] \n", users[temp].login,templogin2,tempmsg); //combinaison chelou pour scanner jusqu'a la fin de la mémoire !
		dprintf(users[temp].socketclient,"[SERVEUR] Message envoyé : %s \n",tempmsg);
		
		for(int i = 0; i<MAX_USERS; i++){
			if(strcmp(templogin2,users[i].login)==0){
				dprintf(users[i].socketclient,"[SERVEUR] Vous avez un message de %s : %s \n",users[temp].login,tempmsg);
			}
		}
	}
	else if(strncmp("<exit>",messageRecu,6) == 0){
		
	}
	
	else{
		printf("Error\n");
		dprintf(users[temp].socketclient, "*-----------------------------------*\n");
		dprintf(users[temp].socketclient, "[SERVEUR] <help> pour avoir les fonctions !\n");
	}
}

