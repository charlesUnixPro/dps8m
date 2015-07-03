# !!!! PLEASE READ THIS BEFORE DOWNLOADING ANY FILES !!!!

There have been a number of misunderstandings about the DPS8 GIT source code tree; sometimes leading to unfortunate user frustration. 

The canonical usage of GIT generally has the master branch containing "releases"; they are expected to be "ready to go". Development takes place on branches; once the development branch is done, it is moved to the master branch as a release. The DPS8 emulator project is not following that model. With just a handful of developers, and any "release" being a hoped-for dream in the unimaginable future, the master branch is the bleeding-edge development code. Commit points on the master branch probably compile on the machine of the person making the commit, but there is no promise of functionality, portability, correctness, update procedures, testing or even that it will compile.

The Alpha 1.0 release was done as a tag on the master branch; this was poor planning on our part as it made it difficult to apply bug fixes to generate a 1.1 version. The Alpha 2.0 release is being done as a branch; this will allow bug fixes to be merged in from the master branch, enabling the creation of release candidates, and future 2.x releases.

As with the master branch please realize that there is no guarantee or implied understanding that release candidates are to work or able to perform any useful function. In this way you, our user community, will not by under any false assumptions about the quality of our release candidate distributions.

The DPS8M team would like to apologize for any difficulties our unorthodox and informal policies may have incurred.



