#!/usr/bin/python

import os, subprocess, sys

class GitRepo:

    def __init__(self, path):
        startdir = os.getcwd()
        # print 'startdir:', startdir
        gitdir = path + '/' + '.git'
        while not os.path.exists( gitdir ) or not os.path.isdir( gitdir ):
            if os.getcwd() == '/': self.__error( '.git never found' )
            os.chdir('..')
            gitdir = os.getcwd() + '/.git'
        self.gitRoot = os.path.dirname(gitdir)
        # print 'self.gitRoot:', self.gitRoot

    def status(self):
        self.__chdir()
        process = subprocess.Popen(['git', 'status'], shell=False, stdout=subprocess.PIPE)
        out, err = process.communicate()
        if out: print out

    def filesSame(self, path, rev1, rev2):
        result = None
        self.__chdir()
        path1 = self.__gitPath(path, rev1)
        path2 = self.__gitPath(path, rev2)
        if path1 and path2:
            params = [ 'git', 'diff', '--no-ext-diff', rev1 + '..' + rev2, '--', path1 ]
            process = subprocess.Popen(params, shell=False, stdout=subprocess.PIPE)
            out, err = process.communicate()
            if err: self.__error( 'git diff failed' )
            result = 0 == len(out)
        elif not path1 and not path2:
            result = True
        else:
            result = False
        return result

    def cat( self, path, rev ):
        result = None
        path = self.__gitPath(path, rev)
        if path:
            params = [ 'git', 'show', rev + ':' + path ]
            process = subprocess.Popen(params, shell=False, stdout=subprocess.PIPE)
            out, err = process.communicate()
            if out: result = out
        return result

    ######################################################################
    # private methods
    ######################################################################
    def __chdir(self): 
        os.chdir(self.gitRoot)

    def __error(self, msg):
        print msg
        sys.exit(1)

    # return full_path if exists for rev and is unique, None otherwise
    def __gitPath(self, path, rev):
        result = []
        params = [ 'git',  'ls-tree', '-r', '--full-name', rev ]
        process = subprocess.Popen(params, shell=False, stdout=subprocess.PIPE)
        out = process.communicate()[0]
        for line in out.splitlines():
            line = line.split()
            if 4 == len(line):
                line = line[3]
                if line.endswith( path ):
                    result.append( line )
        if 1 == len(result): result = result[0]
        else: result = None
        return result

# test function
def main():
    mydir = os.path.dirname(sys.argv[0])
    repo = GitRepo(mydir)
    # print repo.status()

    file ='GamesListDelegate.java'
    print repo.filesSame( file, '4279457471defd66e9e21974772f59944e043db7', \
                          'd463ea3f30909e85acdd7d0e24f00dea15fce81d' )
    print repo.filesSame( file, 'a7c4730eb55f311e20cd406d4b2b819f0cd1edfe', \
                          'd463ea3f30909e85acdd7d0e24f00dea15fce81d' )
    cat = repo.cat( file, 'a7c4730eb55f311e20cd406d4b2b819f0cd1edfe' )
    if cat:
        print 'first line of file %s:' % (file)
        print cat.splitlines()[0]

##############################################################################
if __name__ == '__main__':
    main()
