.PHONY: srpm

srpm:
	./build-srpm.sh "$(outdir)/rpmbuild"
	cp "$(outdir)"/rpmbuild/SRPMS/*.src.rpm "$(outdir)/"
