# mini-backup
A mini application for creating home backups.


## Background
This is mostly meant to be practice with C socket programming and networking and dealing with large files.

I backup a lot of my own data, including photos, music, and films. For this, I have a couple harddrives. I always make sure to have atleast 2 backups of each file and 3 backups if the file is important. I have to do this manually by individually plugging external drives into my laptop and copying the files over. This is pretty annoying, and so this project is meant to address this.

The idea is to have a Raspberry Pi with my external drives plugged into it. Running on the Pi will be mini server that listens for data pushes and will copy pushed data onto multiple drives automatically. This should be accessible from any computer on my network. On my computers, I'll run a client application that interfaces with the Pi. With that application I can push files to the Pi, specify how many backups I want, and receive information like drive capacities, when drives go down, or anything else I might need to know. 

To make this more user friendly, the server will recognize a few categories of file and know where to put those categories and onto how many drives to put them. This makes it simple to keep of track of where files are. The category is specified through the client's command line.

## Server Config

## Usage
The client command line tool as two main methods `--get` and `--put`. The `--get` method is for retrieving data from the backup while `put` is writing data to the backup. Both methods accept several arguments and flags.

Putting a file might look like this:
```
./minib --put --category music --path /path/to/file
```

This will backup the file (either a regular file or a director) according to the rules of the `--music` category, which will be setup during the configuration of the server. If no category is given, the default category will be used (which must exist). If the path points to a directory, all sub-folders will be copied as well.

The `--get` method works very similarly. 

Getting a file might look like this:
```
./minib --get --category music --path /path/to/file
```

Here, the path provided is relative to the rule of the music category. Entire folders can also be retrieved. If no folder or file is given, every file and folder in a given category is retrieved.

There are also the `--info`, `--search` and `--config` methods. `--info` will give information about the configuration of the server. For example:
```
./minib --info --category music
```

This will tell you on which drives and in which folders on those drives music data is stored. This is useful for the `get` method if you don't know the relative paths of files and folders. `info` can also be used to look at usage statistics of drives, drive names, and if any drives are down:
```
./minib --info --drives
```


The `config` method is used to configure the rules of a category. For example:
```
minib --config --category pictures --drives=drive1 --drives=drive2
```

This will create a folder called `pictures` on all indicated drives. From then on, the server will know where to look for all data given the `picture` category--whether that be putting, getting, or info. If the `pictures` category already exists, the above will fail unless the `--refactor` flag is set, in which case all the existing data in the `pictures` category will be moved to match the new storage rules. 

The `--search` function works a bit differently. #TODO
