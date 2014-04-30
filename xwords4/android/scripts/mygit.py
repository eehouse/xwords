#!/usr/bin/python

import logging, os, subprocess, sys
import xwconfig

logging.basicConfig(level=logging.DEBUG
        ,format='%(asctime)s [[%(levelname)s]] %(message)s'
        ,datefmt='%d %b %y %H:%M'
        ,filename='/tmp/mygit.log')
#        ,filemode='w')


class GitRepo:

    def __init__(self, path):
        startPath = path
        logging.debug( '__init__(%s)' % path )
        while True:
            gitdir = path + '/' + '.git'
            print 'looking for', gitdir
            if os.path.exists( gitdir ) and os.path.isdir( gitdir ): break
            elif path == '/': 
                self.__error( '.git never found (starting from %s)' % startPath )
            path = os.path.dirname(path)

        print 'startdir:', path
        self.gitRoot = path
        print 'self.gitRoot:', self.gitRoot

    def status(self):
        process = subprocess.Popen(['git', 'status'], shell=False, \
                                       stdout=subprocess.PIPE, cwd=self.gitRoot)
        out, err = process.communicate()
        if out: 
            return out

    def filesSame(self, path, rev1, rev2):
        result = None
        path1 = self.__gitPath(path, rev1)
        path2 = self.__gitPath(path, rev2)
        if path1 and path2:
            params = [ 'git', 'diff', '--no-ext-diff', rev1 + '..' + rev2, '--', path1 ]
            process = subprocess.Popen(params, shell=False, stdout=subprocess.PIPE, \
                                           cwd = self.gitRoot)
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
            print params
            process = subprocess.Popen(params, shell = False, \
                                           stdout = subprocess.PIPE, \
                                           cwd = self.gitRoot)
            out, err = process.communicate()
            if out: result = out
        else:
            print 'cat: no path'
        return result

    ######################################################################
    # private methods
    ######################################################################
    def __error(self, msg):
        print msg
        sys.exit(1)

    # return full_path if exists for rev and is unique, None otherwise
    def __gitPath(self, path, rev):
        logging.debug('__gitPath(path=%s, rev=%s)' % (path, rev))
        result = []
        params = [ 'git',  'ls-tree', '-r', '--full-name', rev ]
        process = subprocess.Popen(params, shell=False, stdout=subprocess.PIPE, \
                                       cwd = self.gitRoot)
        out = process.communicate()[0]
        for line in out.splitlines():
            line = line.split()
            if 4 == len(line):
                line = line[3]
                if line.endswith( path ):
                    result.append( line )
        if 1 == len(result): result = result[0]
        else: result = None
        logging.debug('__gitPath(%s)=>%s' % (path, result))
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
    file = 'XWords4/archive/R.java'
    cat = repo.cat( file, '33a83b0e2fcf062f4f640ccab0785b2d2b439542' )
    if cat:
        print 'first line of file %s:' % (file)
        print cat.splitlines()[0]
    else:
        print 'cat(%s) failed' % (file)

##############################################################################
if __name__ == '__main__':
    main()
