#!/usr/bin/python

import logging, os, subprocess, sys
import xwconfig

DEBUG = False

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
            # print 'looking for', gitdir
            if os.path.exists( gitdir ) and os.path.isdir( gitdir ): break
            elif path == '/': 
                self.__error( '.git never found (starting from %s)' % startPath )
            oldPath = path
            path = os.path.dirname(path)
            if oldPath == path: 
                self.__error( '.git never found (starting from %s)' % startPath )

        if DEBUG: print 'startdir:', path
        self.gitRoot = path
        if DEBUG: print 'self.gitRoot:', self.gitRoot

    def status(self):
        out, err = self.__doProcess(['git', 'status'])
        if out: 
            return out

    def filesSame(self, path, rev1, rev2):
        result = None
        path1 = self.__gitPath(path, rev1)
        path2 = self.__gitPath(path, rev2)
        if path1 and path2:
            params = [ 'git', 'diff', '--no-ext-diff', rev1 + '..' + rev2, '--', path1 ]
            out, err = self.__doProcess(params)
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
            out, err = self.__doProcess(params)
            if out: result = out
        else:
            print 'cat: no path'
        return result

    def getRevsBetween( self, newRev, oldRev, path = None ):
        params = [ 'git', 'rev-list', newRev ]
        if path:
            path = self.__gitPath( path, newRev )
            if path: params = params + ['--', path]
        out, err = self.__doProcess( params )
        if err: self.__error('error from getRevsBetween')
        result = None
        if out:
            saving = []
            for rev in out.splitlines():
                saving.append(rev)
                if rev == oldRev or newRev == oldRev:
                    result = saving
                    break
        return result;

    def getHeadRev( self ):
        return self.getRevsBetween( 'HEAD', 'HEAD' )[0]

    ######################################################################
    # private methods
    ######################################################################
    def __error(self, msg):
        print msg
        sys.exit(1)

    def __doProcess(self, params):
        process = subprocess.Popen(params, shell = False, \
                                   stdout = subprocess.PIPE, \
                                   cwd = self.gitRoot)
        return process.communicate()

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
    mydir = os.path.dirname(os.path.abspath('.'))
    repo = GitRepo(mydir)
    # print repo.status()

    # file ='GamesListDelegate.java'
    # print repo.filesSame( file, '4279457471defd66e9e21974772f59944e043db7', \
    #                       'd463ea3f30909e85acdd7d0e24f00dea15fce81d' )
    # print repo.filesSame( file, 'a7c4730eb55f311e20cd406d4b2b819f0cd1edfe', \
    #                       'd463ea3f30909e85acdd7d0e24f00dea15fce81d' )
    # file = 'XWords4/archive/R.java'
    # cat = repo.cat( file, '33a83b0e2fcf062f4f640ccab0785b2d2b439542' )
    # if cat:
    #     print 'first line of file %s:' % (file)
    #     print cat.splitlines()[0]
    # else:
    #     print 'cat(%s) failed' % (file)

    # print repo.getRevsBetween( 'a7c4730eb55f311e20cd406d4b2b819f0cd1edfe', \
    #                            '4279457471defd66e9e21974772f59944e043db7', \
    #                            os.path.basename(sys.argv[0] ) )
    hash = 'e643e9d5c064e7572d6132687987c604c61f5cc9'
    print 'looking for', hash
    rJavaRevs = repo.getRevsBetween( 'HEAD', hash, 'R.java')
    print 'rJavaRevs', rJavaRevs
    if 2 < len(rJavaRevs): prev = rJavaRevs[len(rJavaRevs)-2]
    else: prev = None
    print 'first rev before', hash, 'was', prev
    print 'everything between the two:,' 
    print repo.getRevsBetween( prev, hash )

    print 'head:', repo.getHeadRev()


##############################################################################
if __name__ == '__main__':
    main()
