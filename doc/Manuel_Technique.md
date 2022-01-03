---
lang: fr-FR
title: Manuel Technique - CmdS
subtitle: Manuel technique du lanceur de commande CmdS
author:
    - Rémi Labbé
date: Janvier 2022
documentclass: report
toc: true
fontsize: 12
mainfont: Source Code Pro Medium
monofont: mononoki Nerd Font
---

# Introduction

Le manuel technique sert a décrire les différents choix et contraites qui ont été
 rencontrés pendant le développement de ce projet. La spécification des fonctions
 et structures a été faite dans le code lui même.

# Daemon

La création d'un daemon était un exercice nouveau pour moi, je me suis donc
 pour cela aidé de la documentation disponible sur le wiki d'Archlinux (distribution
 de linux que j'utilise) qui décrit la marche à suivre pour la création d'un "SysV Daemon".

[[Archlinux Daemon Manual](https://man.archlinux.org/man/daemon.7)]

Une nouvelle méthode de développement des daemon éxiste pour les rendre controlables
 par le gestionnaire de service du systeme (`systemctl` sur ma machine, `service` sur ubuntu...)
 mais celle ci est plus complexe à mettre en place.

Une des contraintes dont l'implémentation est laissée libre par cette
 documentation est le stockage du pid du daemon, j'ai choisi de le placer dans
 ségment de mémoire partage connu de l'éxécutable du serveur seulement.
 Sa présence suffit à détecter si le daemon est lancé par la même occasion.

Le daemon répond aux signaux utilisateurs pendant sa creation pour vérifier si
 son lancement s'est bien déroulé puis au signal SIGTERM une fois qu'il est en tâche
 de fond, c'est le signal qui sera envoyé pour éteindre le daemon.

Ce daemon gère un pool de thread dans le nombre est spécifié par la macro
 `CAPACITY` dans le fichier **tools/config.h**. Le nombre de threads défini le nombre
 de clients que le daemon peut gérer en meme temps.

Quand un nouveau client se connecte il est confié à un 'runner' qui se chargera
 de communiquer avec lui et d'éxécuter les commandes qu'il envoit. Si une erreur
 se produit, le client sera déconnecté et le thread se terminera pour laisser un
 nouveau client utiliser le runner en question.

Le daemon étant un processus d'arriere plan, aucune sortie sur un terminal ne peut
 être effectuée pour décrire son état. J'ai donc utilisé les logs du systeme,
 accessibles sur ma machine avec la commande `journalctl` je peux trouver les
 lignes qui m'interessent avec la commande suivante:

```bash
journalctl | grep cmds
```

Sur un autre systeme on pourrait utiliser la commande suivante:

```bash
grep cmds /path/to/log_file
```

Pour vérifier que mon processus était bien lancé comme un daemon j'ai pu me fier
 au resultat de la commande:
```bash
ps -xj | grep cmds
```

![ps command screenshot](assets/ps_scrot.png "ps command screenshot")

On peut voir ici que:
- Le TTY = ? ce qui veut dire que le processus n'est plus attaché a un TTY.
- Le PID != SID ce qui veut dire que le processus ne peut pas récuperer le
 controle sur un TTY.

# Client

Le développement du client a été plus simple que celui du daemon. Le but était
 de créer un programme capable de communiquer avec le daemon pour lui demander
 d'éxécuter des commandes. Les moyens de communication seront abordé dans la partie
 suivante.

Une fois connecté au daemon, le lanceur se contentera de lui envoyer ce qu'il lit
 sur son entrée standard.

Il s'arretera, comme dit dans le manuel utilisateur, à la récéption d'un signal
 d'echec envoyé par le daemon, d'un SIGINT (Ctrl+C), d'un SIGQUIT (Ctrl+\\) ou
 de la fin de l'entrée standard (Ctrl+D).

# Communication

La communication entre le daemon et les clients s'effectue grace à 2 élement bien
 distincts.

## File synchronisée

  Dans un premier temps une file synchronisée est créée par le daemon,
 C'est grace à celle ci qu'un client pourra se placer en attente de connection.
 Cette file a été implementée pour contenir des clients, le nombre maximum de clients
 dans la file est défini par la constante `CAPACITY`.

  Cette file a été développée en suivant le modele "Producteur/Consommateur"
 présenté dans le cours, La majeur différence étant que les données sont copiées
 dans la file et non pas inserées, cela évite toute modification menant à une erreur
 du daemon si le client modifie ses informations.

Quelques fonctions supplémentaires ont du être implementées pour créer la file,
 s'y connecter et libérer les ressources une fois la file rendu inutile.

## Tubes

  Si une place peut etre attribuée au client alors un dialogue peut commencer avec
 le runner qui lui a été attribué. 2 tubes sont créés, leur nom est
 unique puisqu'il dépend du PID du client. Un tube est créé à l'insertion du client
 dans la file synchronisée, c'est celui qui servira au client pour parler au daemon.
 Le tube permettant au daemon de répondre est quand a lui recréé a chaque
 requete, la déconnection du daemon de son extremité du tube permet au client de savoir
 que la réponse est complete.

# Limitations

Les commandes sont executées avec les droits que possede l'utilisateur qui a ouvert
 le client. il n'est pas possible d'élever les droits avec sudo une fois le
 client ouvert.

Certaines commande ne sont pas éxécutables par le daemon, par exemple `cd /new/path`.
 Le répértoire de travail restera donc celui dans lequel a été lancé le client.

# Conclusion

La création d'un daemon est un exercice tres intéressant et j'en ai appris un
 peu plus sur les processus sous Linux.
