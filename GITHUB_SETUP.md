# GitHub Setup Instructions

**Repository Name:** `cardputer-adv-tests`  
**Author:** AndyAiCardputer

---

## Step 1: Create Repository on GitHub

1. Go to https://github.com/new
2. **Repository name:** `cardputer-adv-tests`
3. **Description:** `Test sketches for M5Stack Cardputer-Adv - M5Unit-Scroll and other modules`
4. **Visibility:** Public (or Private if you prefer)
5. **DO NOT** initialize with README, .gitignore, or license (we already have them)
6. Click **Create repository**

---

## Step 2: Connect Local Repository to GitHub

After creating repository on GitHub, run these commands:

```bash
cd /Users/a15/A_AI_Project/cardputer-adv-tests

# Add remote (replace YOUR_USERNAME if different)
git remote add origin https://github.com/AndyAiCardputer/cardputer-adv-tests.git

# Verify remote
git remote -v

# Push to GitHub
git branch -M main
git push -u origin main
```

---

## Step 3: Verify

1. Go to https://github.com/AndyAiCardputer/cardputer-adv-tests
2. Check that all files are uploaded:
   - ✅ README.md
   - ✅ .gitignore
   - ✅ tests/m5unit-scroll/
   - ✅ docs/

---

## Future Updates

When you want to add more tests or update existing ones:

```bash
cd /Users/a15/A_AI_Project/cardputer-adv-tests

# Add changes
git add .

# Commit
git commit -m "feat: Add new test for [module name]"

# Push
git push
```

---

## Repository Structure

```
cardputer-adv-tests/
├── README.md                    # Main repository README
├── .gitignore                  # Git ignore rules
├── tests/
│   └── m5unit-scroll/
│       ├── README.md
│       └── unitscroll_test_external_display.ino
└── docs/
    ├── M5UNIT_SCROLL_GUIDE.md
    ├── M5UNIT_SCROLL_USER_GUIDE.md
    └── EXTERNAL_DISPLAY_ILI9488_GUIDE.md
```

---

**Ready to push!** 🚀

