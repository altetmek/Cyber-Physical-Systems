# 2021-group-17

## Setup

### Enviromental configuration

The primary development environment is Ubuntu 20.04 desktop, and it is assumed that this is what you are running.

```Linux
# Update apt
sudo apt update
sudo apt upgrade -y

# Install required packages
sudo apt-get install build-essential cmake git g++
```

[Install Docker](https://www.digitalocean.com/community/tutorials/how-to-install-and-use-docker-on-ubuntu-20-04)

```Linux
sudo apt install apt-transport-https ca-certificates curl software-properties-common
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo apt-key add -
udo add-apt-repository "deb [arch=amd64] https://download.docker.com/linux/ubuntu focal stable"
sudo apt update
apt-cache policy docker-ce
sudo apt install docker-ce docker-compose
sudo systemctl status docker
```

At this point, you should get an output like:

```Linux
● docker.service - Docker Application Container Engine
     Loaded: loaded (/lib/systemd/system/docker.service; enabled; vendor preset>
     Active: active (running) since Thu 2021-03-25 21:08:59 CET; 1 day 19h ago
TriggeredBy: ● docker.socket
       Docs: https://docs.docker.com
   Main PID: 8042 (dockerd)
      Tasks: 13
     Memory: 49.7M
     CGroup: /system.slice/docker.service
             └─8042 /usr/bin/dockerd -H fd:// --containerd=/run/containerd/cont>
```

(Otional) Install [VS Code](https://visualstudio.microsoft.com/vs/community/), the preffered (but not required) IDE.

```Linux
sudo snap install code
```

If you do NOT have an SSH public key, you will need to create one

```Linux
# Only if you do NOT already have an SSH public key
ssh-keygen
cat ./.ssh/id_rsa.pub
```

Copy this output, and add the key to your GitLab account.

### Project configuration

```Linux
# clone the repository
git clone git@git.chalmers.se:courses/dit638/students/2021-group-17.git
cd 2021-group-17.git
code .
```

At this point, the VS Code IDE will open, in the directory of this project.

TODO: Project build instructions tbc

## Group Workflow

### - Add new feature:

- Arrange a working hour with your pair.
- Pick up a task from the GitLab issue boards which is decided within the sprint planning for the pair.
- Create a new Issue if applicable.
- Decide which branch will be used, if a new branch is needed, the pair will create a new one to work in.
- Complete work on the the git board task.
- Commit following the correct commit message protocol.
- Pull/Push.
- If issue is resolved, close issue within GitLab.
- If the feature is complete, conduct a code-review session during the scrum meetings.
- If the the feature gets approved by the team, merge the feature branch into master.

**Note: master branch is always protected.**

### - Fix unexpected behavior in existing features
- When developing a new feature, the team will always create a separate branch from an up-to-date master, since master is always protected.
During the code-review session of an under-development feature, the team shall individually consider if the feature is in-line with what is expected and required. If a disagreement emerge, the team shall follow the code-of-conduct protocol to resolve the disagreement. Once all members share the same point of view, the feature is worked on to fix the unexpected or faulty behavior(s) according to the project specifications. 

- The pair who developed the code which led to unexpected behavior within the feature shall try to identify the bug in the code within a reasonable time-frame.
The pair will try another approach to solve the bug. The pair will research and try to find solutions for a similar problems online. The pair will try to use another algorithm to see if it can resolve the issue. 
If the pair is having trouble/is stuck either with resolving the issue or time, they shall ask for further help from other team members.

## Commit Message Guideline

New features will need to be documented while implementation and double checked after. This includes following a standardized commit message pattern to allow back traceability.
    The format will follow the following rules:

    'reference to issue' + 'description' + 'co-author(s)'
    for example:
    
    resolve #1: finished commit structures.

    Co-authored-by: Leith Hobson <leith@student.chalmers.se>

This format with the knowledge of when the commit was pushed, and the account it was pushed from should allow the group to easily backtrack which team member implemented which function at what time and why.
