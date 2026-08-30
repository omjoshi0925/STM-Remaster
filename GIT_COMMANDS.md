# Copy-paste workflows

## A. Push this bundle to GitHub (Mac Terminal)

    cd ~/Downloads
    unzip -o STM-Remaster_M5_RepoBundle.zip -d STM-Remaster-M5
    git clone https://github.com/omjoshi0925/STM-Remaster.git STM-Remaster-repo || true
    cd STM-Remaster-repo
    git pull origin main
    rsync -av --exclude .git ../STM-Remaster-M5/STM-Remaster/ ./
    git add -A
    git commit -m "Milestone 5: full Level 1, baked vertex lighting, animated thug enemies"
    git push origin main

(GitHub Desktop alternative: File > Add Local Repository > STM-Remaster-repo,
review changes, commit, Push origin.)

## B. From GitHub to a running Xcode build

    cd ~/Downloads/STM-Remaster-repo
    git pull origin main
    ./scripts/apply_to_workbench.sh ~/Downloads/SpiderMan_Native_ARM64_Port_Workbench

Then in Xcode: scheme SpiderManTotalMayhem, device iPhone, Run.

## C. Verify engine logic without Xcode

    cd ~/Downloads/STM-Remaster-repo
    ./hosttests/run_host_tests.sh ~/Downloads/SpiderMan_Native_ARM64_Port_Workbench/Assets
