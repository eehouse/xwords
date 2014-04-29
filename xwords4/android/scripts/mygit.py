#!/usr/bin/python

import os, subprocess

class GitRepo:

    def __init__(self, path):
        self.path = path

    def status(self):
        os.chdir(self.path)
        process = subprocess.Popen(['git', 'status'], shell=False, stdout=subprocess.PIPE)
        result = process.communicate()[0]
        if result: print result
